#ifndef ENGINE_MAIN_H
#define ENGINE_MAIN_H

#include "types.h"
#include "engine_mesh.h"

#define MAX_VERTICES_NUM 8
#define ISINES_NUM 256	// must be power of two
#define ISINES_90_DEG (ISINES_NUM / 4)

#define PROJ_SHR 8
//#define REC_FPSHR 20

extern int *isin;

void initEngine(void);

void transformGeometry(Mesh *ms);
void renderTransformedGeometry(Mesh *ms);

void setScreenDimensions(int w, int h);

#endif
