#include "engine_mesh.h"
#include "engine_texture.h"
#include "engine_main.h"

#include "Doom.h"

static Vertex *vertices;
int *isin;

static int screenWidth = SCREEN_WIDTH;
static int screenHeight = SCREEN_HEIGHT;

static bool polygonOrderTestCPU = true;

static void createRotationMatrixValues(int rotX, int rotY, int rotZ, int *rotVecs)
{
	const int cosxr = isin[(rotX + ISINES_90_DEG) & (ISINES_NUM-1)];
	const int cosyr = isin[(rotY + ISINES_90_DEG) & (ISINES_NUM-1)];
	const int coszr = isin[(rotZ + ISINES_90_DEG) & (ISINES_NUM-1)];
	const int sinxr = isin[rotX & (ISINES_NUM-1)];
	const int sinyr = isin[rotY & (ISINES_NUM-1)];
	const int sinzr = isin[rotZ & (ISINES_NUM-1)];

	*rotVecs++ = (FIXED_MUL(cosyr, coszr, FP_BASE)) << FP_BASE_TO_CORE;
	*rotVecs++ = (FIXED_MUL(FIXED_MUL(sinxr, sinyr, FP_BASE), coszr, FP_BASE) - FIXED_MUL(cosxr, sinzr, FP_BASE)) << FP_BASE_TO_CORE;
	*rotVecs++ = (FIXED_MUL(FIXED_MUL(cosxr, sinyr, FP_BASE), coszr, FP_BASE) + FIXED_MUL(sinxr, sinzr, FP_BASE)) << FP_BASE_TO_CORE;
	*rotVecs++ = (FIXED_MUL(cosyr, sinzr, FP_BASE)) << FP_BASE_TO_CORE;
	*rotVecs++ = (FIXED_MUL(cosxr, coszr, FP_BASE) + FIXED_MUL(FIXED_MUL(sinxr, sinyr, FP_BASE), sinzr, FP_BASE)) << FP_BASE_TO_CORE;
	*rotVecs++ = (FIXED_MUL(-sinxr, coszr, FP_BASE) + FIXED_MUL(FIXED_MUL(cosxr, sinyr, FP_BASE), sinzr, FP_BASE)) << FP_BASE_TO_CORE;
	*rotVecs++ = (-sinyr) << FP_BASE_TO_CORE;
	*rotVecs++ = (FIXED_MUL(sinxr, cosyr, FP_BASE)) << FP_BASE_TO_CORE;
	*rotVecs = (FIXED_MUL(cosxr, cosyr, FP_BASE)) << FP_BASE_TO_CORE;
}

static void translateAndProjectVertices(Mesh *ms)
{
	const int posX = ms->posX;
	const int posY = ms->posY;
	const int posZ = ms->posZ;

	register int i;
	const int lvNum = ms->vrtxNum;

	for (i=0; i<lvNum; i++)
	{
		const int vz = vertices[i].z + posZ;
		if (vz > 0) {
			vertices[i].x = screenWidth / 2 + (((vertices[i].x + posX) << PROJ_SHR) / vz);
			vertices[i].y = screenHeight / 2 - (((vertices[i].y + posY) << PROJ_SHR) / vz);
		}
	}
}

static void rotateVerticesHw(Mesh *ms)
{
	mat33f16 rotMat;

	createRotationMatrixValues(ms->rotX, ms->rotY, ms->rotZ, (int*)rotMat);

	MulManyVec3Mat33_F16((vec3f16*)vertices, (vec3f16*)ms->vrtx, rotMat, ms->vrtxNum);
}

static void renderTransformedGeometryCELs(Mesh *ms)
{
	DrawCels(VideoItem, ms->quad[0].cel);
}

static void prepareTransformedGeometryCELs(Mesh *ms)
{
	int i, j=0;
	int *indices = ms->index;
	Point quad[4];

	int n = 1;
	for (i=0; i<ms->indexNum; i+=4)
	{
		quad[0].pt_X = vertices[indices[i]].x; quad[0].pt_Y = vertices[indices[i]].y;
		quad[1].pt_X = vertices[indices[i+1]].x; quad[1].pt_Y = vertices[indices[i+1]].y;
		quad[2].pt_X = vertices[indices[i+2]].x; quad[2].pt_Y = vertices[indices[i+2]].y;
		quad[3].pt_X = vertices[indices[i+3]].x; quad[3].pt_Y = vertices[indices[i+3]].y;

		if (polygonOrderTestCPU) {
			n = (quad[0].pt_X - quad[1].pt_X) * (quad[2].pt_Y - quad[1].pt_Y) - (quad[2].pt_X - quad[1].pt_X) * (quad[0].pt_Y - quad[1].pt_Y);
		}

		if (!polygonOrderTestCPU || n > 0) {
			ms->quad[j].cel->ccb_Flags &= ~CCB_SKIP;
			MapCel(ms->quad[j].cel, quad);
		} else {
			ms->quad[j].cel->ccb_Flags |= CCB_SKIP;
		}
		++j;
	}
}

static void useCPUtestPolygonOrder(bool enable)
{
	polygonOrderTestCPU = enable;
}

void transformGeometry(Mesh *ms)
{
	rotateVerticesHw(ms);
	translateAndProjectVertices(ms);
}

void drawPixel(int px, int py, uint16 c, uint16 *vram)
{
	uint16 *dst = (uint16*)(vram + (py >> 1) * 320 * 2 + (py & 1) + (px << 1));
	*dst = c;
}

void renderTransformedGeometry(Mesh *ms)
{
	useCPUtestPolygonOrder(ms->useCPUccwTest);

	prepareTransformedGeometryCELs(ms);
	renderTransformedGeometryCELs(ms);
}

void setScreenDimensions(int w, int h)
{
	screenWidth = w;
	screenHeight = h;
}

void initEngine()
{
	uint32 i;

	vertices = (Vertex*)AllocAPointer(MAX_VERTICES_NUM * sizeof(Vertex));
	isin = (int*)AllocAPointer(ISINES_NUM * sizeof(int));

	for(i=0; i<ISINES_NUM; i++) {
		isin[i] = SinF16(i << 16) >> 4;
	}

	useCPUtestPolygonOrder(false);
}
