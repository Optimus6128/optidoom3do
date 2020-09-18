#ifndef ENGINE_GRID_H
#define ENGINE_GRID_H

#include "types.h"

#define MAX_GRID_VERTICES_NUM 81

enum {GRID_FX_DISTORT, GRID_FX_WARP};


typedef struct Point2D
{
    int x, y;
}Point2D;

typedef struct GridMesh
{
	int width;
	int height;

	Point2D *vrtx;
	int vrtxNum;

	int *index;
	int indexNum;

	CCB *cel;
	int celsNum;
}GridMesh;

void initGrid(int gridWidth, int gridHeight);
void updateScreenGridCels(void);
void updateGridFx(int fx, int t);
void renderGrid(void);

#endif
