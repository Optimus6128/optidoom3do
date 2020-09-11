#include "engine_mesh.h"
#include "engine_texture.h"
#include "engine_main.h"

#include "Doom.h"

static Vertex vertices[MAX_VERTICES_NUM];

int icos[256];
int isin[256];

static int shr[257]; // ugly way to get precalced fast right shift for division with power of two numbers
static uint32 recZ[NUM_REC_Z];

static int screenWidth = SCREEN_WIDTH;
static int screenHeight = SCREEN_HEIGHT;

static bool polygonOrderTestCPU = true;

static void(*mapcelFunc)(CCB*, Point*);


static void fasterMapCel(CCB *c, Point *q)
{
	const int shrWidth = shr[c->ccb_Width];
	const int shrHeight = shr[c->ccb_Height];

	const int q0x = q[0].pt_X;
	const int q0y = q[0].pt_Y;
	const int q1x = q[1].pt_X;
	const int q1y = q[1].pt_Y;
	const int q2x = q[2].pt_X;
	const int q2y = q[2].pt_Y;
	const int q3x = q[3].pt_X;
	const int q3y = q[3].pt_Y;

	const int ptX0 = q1x - q0x;
	const int ptY0 = q1y - q0y;
	const int ptX1 = q2x - q3x;
	const int ptY1 = q2y - q3y;
	const int ptX2 = q3x - q0x;
	const int ptY2 = q3y - q0y;

	const int hdx0 = (ptX0 << 20) >> shrWidth;
	const int hdy0 = (ptY0 << 20) >> shrWidth;
	const int hdx1 = (ptX1 << 20) >> shrWidth;
	const int hdy1 = (ptY1 << 20) >> shrWidth;

	c->ccb_XPos = q0x << 16;
	c->ccb_YPos = q0y << 16;

	c->ccb_HDX = hdx0;
	c->ccb_HDY = hdy0;
	c->ccb_VDX = (ptX2 << 16) >> shrHeight;
	c->ccb_VDY = (ptY2 << 16) >> shrHeight;

	c->ccb_HDDX = (hdx1 - hdx0) >> shrHeight;
	c->ccb_HDDY = (hdy1 - hdy0) >> shrHeight;
}

static void createRotationMatrixValues(int rotX, int rotY, int rotZ, int *rotVecs)
{
	const int cosxr = icos[rotX & 255];
	const int cosyr = icos[rotY & 255];
	const int coszr = icos[rotZ & 255];
	const int sinxr = isin[rotX & 255];
	const int sinyr = isin[rotY & 255];
	const int sinzr = isin[rotZ & 255];

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
			const int recDivZ = recZ[vz];
			vertices[i].x = screenWidth / 2 + ((((vertices[i].x + posX) << PROJ_SHR) * recDivZ) >> REC_FPSHR);
			vertices[i].y = screenHeight / 2 - ((((vertices[i].y + posY) << PROJ_SHR) * recDivZ) >> REC_FPSHR);
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
			mapcelFunc(ms->quad[j].cel, quad);
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

static void useMapCelFunctionFast(bool enable)
{
	if (enable) {
		mapcelFunc = fasterMapCel;
	} else {
		mapcelFunc = MapCel;
	}
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
	//uint16 *fuck;
	//int i;
	useMapCelFunctionFast(ms->useFastMapCel);
	useCPUtestPolygonOrder(ms->useCPUccwTest);

	prepareTransformedGeometryCELs(ms);
	renderTransformedGeometryCELs(ms);

	/*fuck = (uint16*)getVideoPointer(WorkPage);
	for (i=0; i<ms->vrtxNum; ++i) {
		drawPixel(vertices[i].x, vertices[i].y, 0x7FFF, fuck);
	}*/
}

void setScreenDimensions(int w, int h)
{
	screenWidth = w;
	screenHeight = h;
}

int getShr(int n)
{
	int b = -1;
	do{
		b++;
	}while((n>>=1)!=0);
	return b;
}

void initEngine()
{
	uint32 i;
	for(i=0; i<256; i++) {
		isin[i] = SinF16(i << 16) >> 4;
		icos[i] = CosF16(i << 16) >> 4;
	}

	for (i=1; i<=256; i++) {
		shr[i] = getShr(i);
	}

	for (i=1; i<NUM_REC_Z; ++i) {
		recZ[i] = (1 << REC_FPSHR) / i;
	}

	useCPUtestPolygonOrder(false);
	useMapCelFunctionFast(true);
}
