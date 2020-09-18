#ifndef ENGINE_MESH_H
#define ENGINE_MESH_H

#include "types.h"
#include "engine_texture.h"

#include <celutils.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

#define MESH_OPTION_FAST_MAPCEL		(1 << 0)
#define MESH_OPTION_CPU_CCW_TEST	(1 << 1)
#define MESH_OPTIONS_DEFAULT (MESH_OPTION_FAST_MAPCEL | MESH_OPTION_CPU_CCW_TEST)

#define FP_CORE 16
#define FP_BASE 12
#define FP_BASE_TO_CORE (FP_CORE - FP_BASE)

#define FLOAT_TO_FIXED(f,b) ((int)((f) * (1 << b)))
#define INT_TO_FIXED(i,b) ((i) * (1 << b))
#define UINT_TO_FIXED(i,b) ((i) << b)
#define FIXED_TO_INT(x,b) ((x) >> b)
#define FIXED_TO_FLOAT(x,b) ((float)(x) / (1 << b))
#define FIXED_MUL(x,y,b) (((x) * (y)) >> b)
#define FIXED_DIV(x,y,b) (((x) << b) / (y))
#define FIXED_SQRT(x,b) (sqrt((x) << b))

#define PI 3.14159265359f
#define DEG256RAD ((2 * PI) / 256.0f)


typedef struct Vertex
{
	int x, y, z;
}Vertex;

typedef struct QuadData
{
	CCB *cel;
	ubyte textureId;
	ubyte palId;
}QuadData;

typedef struct Mesh
{
	Vertex *vrtx;
	int vrtxNum;

	int *index;
	int indexNum;

	QuadData *quad;
	int quadsNum;

	Texture *tex;
	ubyte texturesNum;

	int posX, posY, posZ;
	int rotX, rotY, rotZ;

	bool useFastMapCel;
	bool useCPUccwTest;
}Mesh;


//extern int shr[257];


Mesh* initMesh(int vrtxNum, int quadsNum);
// TODO: Mesh *loadMesh(char *path);

void prepareCelList(Mesh *ms);

void setMeshPosition(Mesh *ms, int px, int py, int pz);
void setMeshRotation(Mesh *ms, int rx, int ry, int rz);
void setMeshPolygonOrder(Mesh *ms, bool cw, bool ccw);
void setMeshTranslucency(Mesh *ms, bool enable);

void updateMeshCELs(Mesh *ms);

#endif
