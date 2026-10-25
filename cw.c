#include "graphics.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

int direction = 0;
int xCoords[3] = {185, 175, 195};
int yCoords[3] = {176, 194, 194};
const int delay = 200;

const int CELL = 30;
const int BORDER_LEFT = 50;
const int BORDER_TOP  = 50;
int rows = 9, cols = 9;
int arenaLeft  = BORDER_LEFT;
int arenaTop   = BORDER_TOP;
int arenaRight = 350;
int arenaBottom= 350;

#define MAXRC 20
#define MAX_MARKERS 12

int worldBlocked[MAXRC][MAXRC];
int worldHasMarker[MAXRC][MAXRC];

enum { UNKNOWN=-1, FREE=0, BLOCKED=1 };
int known[MAXRC][MAXRC]; 
int visitedCell[MAXRC][MAXRC];

int carryingCount = 0;
int droppedTotal = 0;
int pileRow = -1, pileCol = -1;

int rnd(int a, int b) { return a + rand() % (b - a + 1); }
static inline int centerX(int c) { return arenaLeft + c * CELL + CELL/2; }
static inline int centerY(int r) { return arenaTop  + r * CELL + CELL/2; }

static inline int clamp(int v, int lo, int hi) { return v<lo?lo:(v>hi?hi:v); }
static inline int tipCol() { return clamp((xCoords[0] - arenaLeft) / CELL, 0, cols-1); }
static inline int tipRow() { return clamp((yCoords[0] - arenaTop)  / CELL, 0, rows-1); }
int inBounds(int r, int c){ return r>=0 && r<rows && c>=0 && c<cols; }

void draw_static_background()
{
    background();
    setColour(red);
    fillRect(arenaLeft - 3, arenaTop - 3, cols*CELL + 6, rows*CELL + 6);
    setColour(white);
    fillRect(arenaLeft, arenaTop, cols*CELL, rows*CELL);

    setColour(gray);
    for (int x = arenaLeft + CELL; x < arenaRight; x += CELL)
        drawLine(x, arenaTop, x, arenaBottom);
    for (int y = arenaTop + CELL; y < arenaBottom; y += CELL)
        drawLine(arenaLeft, y, arenaRight, y);
    for (int r=0;r<rows;r++)
        for (int c=0;c<cols;c++)
            if (worldBlocked[r][c]) {
                setColour(black);
                fillRect(arenaLeft + c*CELL+1, arenaTop + r*CELL+1, CELL-2, CELL-2);
            }
}

void draw_markers()
{
    setColour(gray);
    for (int r=0;r<rows;r++)
        for (int c=0;c<cols;c++)
            if (worldHasMarker[r][c])
                fillOval(centerX(c)-10, centerY(r)-10, 20, 20);

    if (pileRow>=0 && pileCol>=0 && carryingCount==0 && droppedTotal>0){
        setColour(gray);
        fillOval(centerX(pileCol)-10, centerY(pileRow)-10, 20, 20);
        setColour(black);
        char buf[8]; sprintf(buf, "%d", droppedTotal);
        drawString(buf, centerX(pileCol)-4, centerY(pileRow)+4);
    }
}

void set_triangle_from_tip(int tipx, int tipy, int dir)
{
    int h=10,b=14;
    if (dir == 0) {
        xCoords[0]=tipx; yCoords[0]=tipy;
        xCoords[1]=tipx-h; yCoords[1]=tipy+b;
        xCoords[2]=tipx+h; yCoords[2]=tipy+b;
    } else if (dir == 1) {
        xCoords[0]=tipx; yCoords[0]=tipy;
        xCoords[1]=tipx-b; yCoords[1]=tipy-h;
        xCoords[2]=tipx-b; yCoords[2]=tipy+h;
    } else if (dir == 2) {
        xCoords[0]=tipx; yCoords[0]=tipy;
        xCoords[1]=tipx-h; yCoords[1]=tipy-b;
        xCoords[2]=tipx+h; yCoords[2]=tipy-b;
    } else {
        xCoords[0]=tipx; yCoords[0]=tipy;
        xCoords[1]=tipx+b; yCoords[1]=tipy-h;
        xCoords[2]=tipx+b; yCoords[2]=tipy+h;
    }
}

void update()
{
    foreground();
    clear();
    draw_markers();
    setColour(red);
    fillPolygon(3, xCoords, yCoords);
}

void face(int wantDir)
{
    wantDir = ((wantDir % 4) + 4) % 4;
    direction = wantDir;
    set_triangle_from_tip(xCoords[0], yCoords[0], direction);
    update();
}

int tryStepForward()
{
    int r = tipRow(), c = tipCol();
    int nr=r, nc=c;
    if (direction==0) nr--;
    else if (direction==1) nc++;
    else if (direction==2) nr++;
    else if (direction==3) nc--;

    if (!inBounds(nr,nc)) { known[nr][nc] = BLOCKED; return 0; }

    if (worldBlocked[nr][nc]) {
        known[nr][nc] = BLOCKED;
        return 0;
    }

    if (direction == 0) for (int i=0;i<3;i++) yCoords[i] -= CELL;
    else if (direction == 1) for (int i=0;i<3;i++) xCoords[i] += CELL;
    else if (direction == 2) for (int i=0;i<3;i++) yCoords[i] += CELL;
    else if (direction == 3) for (int i=0;i<3;i++) xCoords[i] -= CELL;

    known[nr][nc]   = FREE;
    visitedCell[nr][nc] = 1;
    update();
    return 1;
}

void tryPickUpHere()
{
    int r = tipRow(), c = tipCol();
    if (worldHasMarker[r][c]){
        worldHasMarker[r][c] = 0;
        carryingCount++;
        update();
    }
}

typedef struct { int r,c; } Node;

int bfs_known_free(int sr,int sc,int tr,int tc, Node path[], int *plen)
{
    static int vis[MAXRC][MAXRC];
    static int prevDir[MAXRC][MAXRC];
    memset(vis, 0, sizeof(vis));
    for (int i=0;i<MAXRC;i++) for (int j=0;j<MAXRC;j++) prevDir[i][j]=-1;

    Node q[MAXRC*MAXRC];
    int qh=0, qt=0;

    if (known[sr][sc]!=FREE) return 0;
    vis[sr][sc]=1;
    q[qt++] = (Node){sr,sc};

    int dr[4]={-1,0,1,0};
    int dc[4]={0,1,0,-1};

    while (qh<qt){
        Node cur = q[qh++];
        if (cur.r==tr && cur.c==tc){
            Node rev[MAXRC*MAXRC];
            int len=0, r=tr, c=tc;
            while (!(r==sr && c==sc)){
                rev[len++] = (Node){r,c};
                int d = prevDir[r][c];
                if (d==0) r++; else if (d==1) c--; else if (d==2) r--; else if (d==3) c++;
                if (len > rows*cols+5) return 0;
            }
            for (int i=len-1,j=0; i>=0; --i, ++j) path[j]=rev[i];
            *plen = len;
            return 1;
        }
        for (int d=0; d<4; d++){
            int nr = cur.r + dr[d], nc = cur.c + dc[d];
            if (!inBounds(nr,nc)) continue;
            if (known[nr][nc] != FREE) continue;
            if (vis[nr][nc]) continue;
            vis[nr][nc]=1;
            prevDir[nr][nc]=d;
            q[qt++] = (Node){nr,nc};
        }
    }
    return 0;
}

void follow_known_path(Node path[], int plen)
{
    for (int i=0;i<plen;i++){
        int nr = path[i].r, nc = path[i].c;
        int r = tipRow(), c = tipCol();
        if (nr==r && nc==c) continue;

        if (nr==r-1 && nc==c) face(0);
        else if (nr==r && nc==c+1) face(1);
        else if (nr==r+1 && nc==c) face(2);
        else if (nr==r && nc==c-1) face(3);
        else return;

        if (!tryStepForward()) return;
        tryPickUpHere();
        sleep(delay);
    }
}

int find_nearest_frontier_adj(Node path[], int *plen, int *adjR, int *adjC, int *unkR, int *unkC)
{
    int sr=tipRow(), sc=tipCol();
    int bestLen = 1e9;
    Node bestPath[MAXRC*MAXRC]; int bestPlen=0;
    int bestAdjR=-1,bestAdjC=-1,bestUnkR=-1,bestUnkC=-1;

    for (int r=0;r<rows;r++) for (int c=0;c<cols;c++){
        if (known[r][c] != FREE) continue;
        int dr[4]={-1,0,1,0};
        int dc[4]={0,1,0,-1};
        for (int d=0; d<4; d++){
            int ur=r+dr[d], uc=c+dc[d];
            if (!inBounds(ur,uc)) continue;
            if (known[ur][uc] != UNKNOWN) continue;
            Node tmp[MAXRC*MAXRC]; int tlen=0;
            if (bfs_known_free(sr,sc,r,c,tmp,&tlen)){
                if (tlen < bestLen){
                    bestLen=tlen; bestPlen=tlen;
                    memcpy(bestPath, tmp, sizeof(Node)*tlen);
                    bestAdjR=r; bestAdjC=c;
                    bestUnkR=ur; bestUnkC=uc;
                }
            }
        }
    }

    if (bestLen<1e9){
        *plen = bestPlen;
        memcpy(path, bestPath, sizeof(Node)*bestPlen);
        *adjR = bestAdjR; *adjC = bestAdjC;
        *unkR = bestUnkR; *unkC = bestUnkC;
        return 1;
    }
    return 0;
}

void step_into_unknown_if_adj(int unkR, int unkC)
{
    int r=tipRow(), c=tipCol();
    if (unkR==r-1 && unkC==c) face(0);
    else if (unkR==r && unkC==c+1) face(1);
    else if (unkR==r+1 && unkC==c) face(2);
    else if (unkR==r && unkC==c-1) face(3);
    else return;

    if (tryStepForward()){
        tryPickUpHere();
        sleep(delay);
    } else {
    }
}

int go_to_nearest_reachable_corner(Node path[], int *plen)
{
    int sr=tipRow(), sc=tipCol();
    int cr[4]={0,0,rows-1,rows-1};
    int cc[4]={0,cols-1,0,cols-1};
    int bestLen=1e9, bi=-1;
    Node bestPath[MAXRC*MAXRC]; int blen=0;

    for (int i=0;i<4;i++){
        int r=cr[i], c=cc[i];
        if (!inBounds(r,c)) continue;
        if (known[r][c] != FREE) continue;
        Node tmp[MAXRC*MAXRC]; int tlen=0;
        if (bfs_known_free(sr,sc,r,c,tmp,&tlen)){
            if (tlen<bestLen){ bestLen=tlen; bi=i; blen=tlen; memcpy(bestPath,tmp,sizeof(Node)*tlen); }
        }
    }
    if (bi!=-1){
        *plen = blen; memcpy(path,bestPath,sizeof(Node)*blen);
        return 1;
    }
    return 0;
}

void set_robot_at_grid(int gx, int gy, int dir)
{
    int tx = arenaLeft + gx * CELL + CELL/2;
    int ty = arenaTop  + gy * CELL + CELL/2;
    direction = dir;
    set_triangle_from_tip(tx, ty, direction);
    update();
}

void randomize_world()
{
    int total = rows*cols;
    int obs = rnd(total/10, total/4);

    memset(worldBlocked, 0, sizeof(worldBlocked));
    memset(worldHasMarker,0, sizeof(worldHasMarker));
    memset(known, UNKNOWN, sizeof(known));
    memset(visitedCell, 0, sizeof(visitedCell));

    int rcR[4]={0,0,rows-1,rows-1}, rcC[4]={0,cols-1,0,cols-1};

    int placed=0, guard=0;
    while (placed<obs && guard<10000){
        guard++;
        int r=rnd(0,rows-1), c=rnd(0,cols-1);
        int isCorner = ((r==rcR[0]&&c==rcC[0]) || (r==rcR[1]&&c==rcC[1]) || (r==rcR[2]&&c==rcC[2]) || (r==rcR[3]&&c==rcC[3]));
        if (isCorner) continue;
        if (!worldBlocked[r][c]){
            worldBlocked[r][c]=1;
            placed++;
        }
    }

    int totalMarkers = rnd(2, (total/2 < MAX_MARKERS ? total/2 : MAX_MARKERS));
    int mplaced=0;
    guard=0;
    while (mplaced<totalMarkers && guard<10000){
        guard++;
        int r=rnd(0,rows-1), c=rnd(0,cols-1);
        if (!worldBlocked[r][c] && !worldHasMarker[r][c]){
            worldHasMarker[r][c]=1;
            mplaced++;
        }
    }
}

int main()
{
    srand((unsigned int)time(NULL));
    setWindowSize(500, 500);

    rows = rnd(5,10);
    cols = rnd(5,10);
    if (rows>MAXRC) rows=MAXRC;
    if (cols>MAXRC) cols=MAXRC;

    arenaRight  = arenaLeft + cols*CELL;
    arenaBottom = arenaTop  + rows*CELL;

    randomize_world();
    draw_static_background();

    int rgx=0, rgy=0, rdir=0, guard=0;
    do {
        rgx = rnd(0, cols-1);
        rgy = rnd(0, rows-1);
        rdir= rnd(0,3);
        guard++;
        if (guard>10000) break;
    } while (worldBlocked[rgy][rgx]);
    set_robot_at_grid(rgx, rgy, rdir);

    known[rgy][rgx] = FREE;
    visitedCell[rgy][rgx] = 1;
    tryPickUpHere();

    while (1){
        Node toAdj[MAXRC*MAXRC]; int plen=0, adjR=0, adjC=0, unkR=0, unkC=0;
        if (!find_nearest_frontier_adj(toAdj,&plen,&adjR,&adjC,&unkR,&unkC)) break; 
        follow_known_path(toAdj, plen); 
        if (!(tipRow()==adjR && tipCol()==adjC)) continue;
        step_into_unknown_if_adj(unkR, unkC);
    }

    Node toCorner[MAXRC*MAXRC]; int clen=0;
    if (go_to_nearest_reachable_corner(toCorner,&clen)){
        follow_known_path(toCorner, clen);
    }

    int r=tipRow(), c=tipCol();
    pileRow=r; pileCol=c;
    droppedTotal = carryingCount;
    carryingCount = 0;
    update();

    sleep(2000);
    return 0;
}
