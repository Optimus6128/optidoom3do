#ifndef ENGINE_GRID_H
#define ENGINE_GRID_H

#include "types.h"
#include "engine_mesh.h"
#include "engine_main.h"

#define MAX_GRID_VERTICES_NUM 256

enum {GRID_FX_DISTORT, GRID_FX_WARP};

typedef struct GridMesh
{
	int width;
	int height;
	Mesh *mesh;
}GridMesh;

void initGrid(int gridWidth, int gridHeight);
void updateScreenGridCels(void);
void updateGridFx(int fx, int t);
void renderGrid(void);

#endif
