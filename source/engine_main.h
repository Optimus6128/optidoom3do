#ifndef ENGINE_MAIN_H
#define ENGINE_MAIN_H

#include "types.h"
#include "engine_mesh.h"

#define MAX_VERTICES_NUM 16

#define PROJ_SHR 8
#define REC_FPSHR 20
#define NUM_REC_Z 32768

extern int icos[256];
extern int isin[256];


void initEngine(void);

void transformGeometry(Mesh *ms);
void renderTransformedGeometry(Mesh *ms);

void setScreenDimensions(int w, int h);
int getShr(int n);

#endif
