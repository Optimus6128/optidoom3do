#include "engine_main.h"
#include "engine_mesh.h"
#include "engine_grid.h"

#include "Doom.h"


static Vertex gridVertices[MAX_GRID_VERTICES_NUM];

static GridMesh *gridMesh;


static void recalculateGridMeshVertices()
{
	int i, x, y;
	int xp, yp;
	int dx, dy;

	Mesh *ms = gridMesh->mesh;

	dx = ScreenWidth / gridMesh->width;
	dy = ScreenHeight / gridMesh->height;

	i = 0;
	yp = 0;
	for (y=0; y<=gridMesh->height; y++)
	{
		xp = 0;
		for (x=0; x<=gridMesh->width; x++)
		{
			ms->vrtx[i].x = ScreenXOffsetUnscaled + xp; ms->vrtx[i].y= ScreenYOffsetUnscaled + ScreenHeight - yp; ms->vrtx[i].z = 0;
			xp += dx;
			i++;
		}
		yp += dy;
	}
}

static void updateGridCel(int posX, int posY, int width, int height, unsigned char *vram, CCB *cel)
{
	const int screenStartOffset = ((posY >> 1) * 320 * 2 + (posY & 1) + (posX << 1)) * 2;
	const int woffset = 320 - 2;
	const int vcnt = (height / 2) - 1;
	unsigned char *offscreenVramPointer = (unsigned char*)(vram + screenStartOffset);

	cel->ccb_SourcePtr = (CelData*)offscreenVramPointer;
	cel->ccb_PRE0 = (cel->ccb_PRE0 & ~(((1<<10) - 1)<<6)) | (vcnt << 6);
	cel->ccb_PRE1 = (cel->ccb_PRE1 & (65536 - 1024)) | (woffset << 16) | (width-1);
	cel->ccb_Width = width;
	cel->ccb_Height = height;
}


void updateScreenGridCels()
{
	int i;
	const int sizeX = ScreenWidth / gridMesh->width;
	const int sizeY = ScreenHeight / gridMesh->height;
	unsigned char *vram = getVideoPointer(offscreenPage);

	Mesh *ms = gridMesh->mesh;

	recalculateGridMeshVertices();

	for (i=0; i<ms->quadsNum; ++i) {
		QuadData *qd = &ms->quad[i];
		Vertex *v = &ms->vrtx[ms->index[i<<2]];

		updateGridCel(v->x, v->y, sizeX, sizeY, vram, qd->cel);
	}
}

static void initGridCelList(Mesh *ms)
{
	int i;
	for (i=0; i<ms->quadsNum; i++)
	{
		ms->quad[i].cel = (CCB*)AllocMem(sizeof(CCB), MEMTYPE_ANY);

		ms->quad[i].cel->ccb_Flags = CCB_NPABS | CCB_SPABS | CCB_PPABS | CCB_LDSIZE | CCB_LDPRS | CCB_LDPPMP | CCB_CCBPRE | CCB_YOXY | CCB_USEAV | CCB_NOBLK | CCB_ACE | CCB_ACW | CCB_ACCW | CCB_ACSC | CCB_ALSC | CCB_BGND;
		ms->quad[i].cel->ccb_PRE0 = PRE0_LINEAR | 6;				// PRE0_LINEAR is the bit for UNCODED really, 6 for 16bit
		ms->quad[i].cel->ccb_PRE1 = PRE1_TLLSB_PDC0 | PRE1_LRFORM;	// MSB of blue remains, VRAM structured texture format
		ms->quad[i].cel->ccb_PIXC = 0x1F001F00;

		if (i!=0) LinkCel(ms->quad[i-1].cel, ms->quad[i].cel);
	}
	ms->quad[ms->quadsNum-1].cel->ccb_Flags |= CCB_LAST;
}

static GridMesh *initGridMesh(int gridWidth, int gridHeight)
{
	GridMesh *gms = (GridMesh*)AllocMem(sizeof(GridMesh), MEMTYPE_ANY);

	int i, x, y;

	Mesh *ms;

	const int vrtxNum = (gridWidth + 1) * (gridHeight + 1);
	const int quadsNum = gridWidth * gridHeight;

	ms = initMesh(vrtxNum, quadsNum);

	i = 0;
	for (y=0; y<gridHeight; y++)
	{
		for (x=0; x<gridWidth; x++)
		{
			ms->index[i+3] = x + y * (gridWidth + 1);
			ms->index[i+2] = x + 1 + y * (gridWidth + 1);
			ms->index[i+1] = x + 1 + (y + 1) * (gridWidth + 1);
			ms->index[i] = x + (y + 1) * (gridWidth + 1);
			i+=4;
		}
	}

	initGridCelList(ms);
	updateScreenGridCels();

	gms->mesh = ms;
	gms->width = gridWidth;
	gms->height = gridHeight;

	return gms;
}

static void warpGridVertices(int t)
{
	Mesh *ms = gridMesh->mesh;

	int i;
	const int lvNum = ms->vrtxNum;
	const int gravity = isin[(t << 1) & 255] + 4096;	// range 0 to 8192
	const int centerX = ScreenXOffsetUnscaled + ScreenWidth / 2;
	const int centerY = ScreenYOffsetUnscaled + ScreenHeight / 2;

	for (i=0; i<lvNum; i++)
	{
		const int dx = ms->vrtx[i].x - centerX;
		const int dy = ms->vrtx[i].y - centerY;


		/*int radius = dx*dx + dy*dy;
		int invRadius;
		if (radius==0) radius = 1;
		invRadius = 32768 / radius;
		gridVertices[i].x = ms->vrtx[i].x + (((dx * invRadius * gravity) >> 13) >> 5);
		gridVertices[i].y = ms->vrtx[i].y + (((dy * invRadius * gravity) >> 13) >> 5);*/


		int radius = (dx*dx + dy*dy) >> 4;
		gridVertices[i].x = ms->vrtx[i].x + (((dx * radius * gravity) >> 13) >> 7);
		gridVertices[i].y = ms->vrtx[i].y + (((dy * radius * gravity) >> 13) >> 7);
	}
}

static void distortGridVertices(int t)
{
	Mesh *ms = gridMesh->mesh;

	int i;
	const int lvNum = ms->vrtxNum;

	for (i=0; i<lvNum; i++)
	{
		gridVertices[i].x = ms->vrtx[i].x + (isin[(i*t) & 255] >> 10);
		gridVertices[i].y = ms->vrtx[i].y + (icos[(2*i*t) & 255] >> 10);
	}
}

static void renderGridCELs()
{
	DrawCels(VideoItem, gridMesh->mesh->quad[0].cel);
}

static void prepareGridCELs()
{
	Mesh *ms = gridMesh->mesh;

	int i, j=0;
	int *indices = ms->index;
	Point quad[4];

	for (i=0; i<ms->indexNum; i+=4)
	{
		quad[0].pt_X = gridVertices[indices[i]].x; quad[0].pt_Y = gridVertices[indices[i]].y;
		quad[1].pt_X = gridVertices[indices[i+1]].x; quad[1].pt_Y = gridVertices[indices[i+1]].y;
		quad[2].pt_X = gridVertices[indices[i+2]].x; quad[2].pt_Y = gridVertices[indices[i+2]].y;
		quad[3].pt_X = gridVertices[indices[i+3]].x; quad[3].pt_Y = gridVertices[indices[i+3]].y;

		MapCel(ms->quad[j++].cel, quad);
	}
}

void updateGridFx(int fx, int t)
{
	switch(fx)
	{
		case GRID_FX_DISTORT:
			distortGridVertices(t);
		break;

		case GRID_FX_WARP:
			warpGridVertices(t);
		break;

		default:
		break;
	}
}

void renderGrid()
{
	prepareGridCELs();
	renderGridCELs();
}

void initGrid(int gridWidth, int gridHeight)
{
	gridMesh = initGridMesh(gridWidth, gridHeight);
}
