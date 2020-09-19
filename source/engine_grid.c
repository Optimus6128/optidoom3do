#include "engine_main.h"
#include "engine_grid.h"

#include "Doom.h"
#include "3dlib.h"

static Point2D *gridVertices = NULL;

static GridMesh *gridMesh = NULL;


static void recalculateGridMeshVertices()
{
	int i, x, y;
	int xp, yp;
	int dx, dy;

	int ScreenXOffsetFinal = ScreenXOffsetUnscaled;
	int ScreenYOffsetFinal = ScreenYOffsetUnscaled;
	int ScreenWidthFinal = ScreenWidthUnscaled;
	int ScreenHeightFinal = ScreenHeightUnscaled;

	if (opt_fitToScreen) {
		ScreenXOffsetFinal = 0;
		ScreenYOffsetFinal = 0;
		ScreenWidthFinal = 320;
		ScreenHeightFinal = 160;
	}

	dx = ScreenWidthFinal / gridMesh->width;
	dy = ScreenHeightFinal / gridMesh->height;

	i = 0;
	yp = 0;
	for (y=0; y<=gridMesh->height; y++) {
		xp = 0;
		for (x=0; x<=gridMesh->width; x++) {
			gridMesh->vrtx[i].x = ScreenXOffsetFinal + xp;
			gridMesh->vrtx[i].y= ScreenYOffsetFinal + ScreenHeightFinal - yp;
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
	cel->ccb_PRE0 = PRE0_LINEAR | 6 | (vcnt << 6);									// PRE0_LINEAR is the bit for UNCODED really, 6 for 16bit
	cel->ccb_PRE1 = PRE1_TLLSB_PDC0 | PRE1_LRFORM | (woffset << 16) | (width-1);	// MSB of blue remains, VRAM structured texture format
	cel->ccb_Width = width;
	cel->ccb_Height = height;
}


void updateScreenGridCels()
{
	int i, x, y;
	int xp, yp;
	int vx, vy;

	const int sizeX = ScreenWidth / gridMesh->width;
	const int sizeY = ScreenHeight / gridMesh->height;
	unsigned char *vram = getVideoPointer(offscreenPage);

	// Small fix(not perfect) for small screen sizes that also under halfY scale don't divide well by 8*8 grid
	int evenSizeY = sizeY;
	if (evenSizeY & 1) ++evenSizeY;

	i = 0;
	yp = 0;
	for (y=0; y<gridMesh->height; y++) {
		xp = 0;
		for (x=0; x<gridMesh->width; x++) {
			CCB *cel = &gridMesh->cel[i++];

			vx = ScreenXOffset + xp;
			vy= ScreenYOffset + ScreenHeight - sizeY - yp;
			xp += sizeX;

			updateGridCel(vx, vy, sizeX, evenSizeY, vram, cel);
		}
		yp += sizeY;
	}

	recalculateGridMeshVertices();
}

static GridMesh *initGridMesh(int gridWidth, int gridHeight)
{
	GridMesh *gms = (GridMesh*)AllocAPointer(sizeof(GridMesh));

	int i, x, y;

	const int vrtxNum = (gridWidth + 1) * (gridHeight + 1);
	const int celsNum = gridWidth * gridHeight;

	gms->vrtxNum = vrtxNum;
	gms->celsNum = celsNum;

	gms->indexNum = gms->celsNum << 2;
	gms->vrtx = (Point2D*)AllocAPointer(gms->vrtxNum * sizeof(Point2D));
	gms->index = (int*)AllocAPointer(gms->indexNum * sizeof(int));
	gms->cel = (CCB*)AllocAPointer(gms->celsNum * sizeof(CCB));

	i = 0;
	for (y=0; y<gridHeight; y++)
	{
		for (x=0; x<gridWidth; x++)
		{
			gms->index[i+3] = x + y * (gridWidth + 1);
			gms->index[i+2] = x + 1 + y * (gridWidth + 1);
			gms->index[i+1] = x + 1 + (y + 1) * (gridWidth + 1);
			gms->index[i] = x + (y + 1) * (gridWidth + 1);
			i+=4;
		}
	}

	for (i=0; i<gms->celsNum; i++)
	{
		gms->cel[i].ccb_Flags = CCB_NPABS | CCB_SPABS | CCB_PPABS | CCB_LDSIZE | CCB_LDPRS | CCB_LDPPMP | CCB_CCBPRE | CCB_YOXY | CCB_USEAV | CCB_NOBLK | CCB_ACE | CCB_ACW | CCB_ACCW | CCB_ACSC | CCB_ALSC | CCB_BGND;
		gms->cel[i].ccb_PIXC = PIXC_OPAQUE;

		if (i!=0) LinkCel(&gms->cel[i-1], &gms->cel[i]);
	}
	gms->cel[gms->celsNum-1].ccb_Flags |= CCB_LAST;

	gms->width = gridWidth;
	gms->height = gridHeight;

	return gms;
}

static void warpGridVertices(int t)
{
	const int xCount = gridMesh->width + 1;
	const int yCount = gridMesh->height + 1;

	const int gravity = isin[(t << 1) & 255] + 4096;	// range 0 to 8192
	const int centerX = ScreenXOffsetUnscaled + ScreenWidthUnscaled / 2;
	const int centerY = ScreenYOffsetUnscaled + ScreenHeightUnscaled / 2;

	int x, y;
	int i = 0;
	for (y=0; y<yCount; ++y) {
		for (x=0; x<xCount; ++x) {
			int vx, vy;
			const int dx = gridMesh->vrtx[i].x - centerX;
			const int dy = gridMesh->vrtx[i].y - centerY;

			/*int radius = dx*dx + dy*dy;
			int invRadius;
			if (radius==0) radius = 1;
			invRadius = 32768 / radius;
			vx = gridMesh->vrtx[i].x + (((dx * invRadius * gravity) >> 13) >> 5);
			vy = gridMesh->vrtx[i].y + (((dy * invRadius * gravity) >> 13) >> 5);*/


			int radius = (dx*dx + dy*dy) >> 4;
			vx = gridMesh->vrtx[i].x + (((dx * radius * gravity) >> 13) >> 8);
			vy = gridMesh->vrtx[i].y + (((dy * radius * gravity) >> 13) >> 8);

			gridVertices[i].x = vx;
			gridVertices[i].y = vy;
			++i;
		}
	}
}

static void distortGridVertices(int t)
{
	const int xCount = gridMesh->width + 1;
	const int yCount = gridMesh->height + 1;

	int x, y;
	int i = 0;
	for (y=0; y<yCount; ++y) {
		const int lvy = ((2*i*y + 3*i*y*y) % 11) * 2 * t;
		for (x=0; x<xCount; ++x) {
			if (x==0 || x==gridMesh->width || y==0 || y==gridMesh->height) {
				gridVertices[i].x = gridMesh->vrtx[i].x-1;
				gridVertices[i].y = gridMesh->vrtx[i].y-1;
			} else {
				const int lvx = ((3*i*x + 5*i*x*x) & 15) * t;
				gridVertices[i].x = gridMesh->vrtx[i].x + (isin[lvy & 255] >> 10);
				gridVertices[i].y = gridMesh->vrtx[i].y + (isin[(lvx + ISINES_90_DEG) & 255] >> 10);
			}
			++i;
		}
	}
}

static void renderGridCELs()
{
	DrawCels(VideoItem, &gridMesh->cel[0]);
}

static void prepareGridCELs()
{
	int i, j=0;
	int *indices = gridMesh->index;
	Point quad[4];

	int smallOffset = 1;
	if (opt_gimmicks == GIMMICKS_MOTION_BLUR) smallOffset = 0;

	for (i=0; i<gridMesh->indexNum; i+=4)
	{
		quad[0].pt_X = gridVertices[indices[i]].x; quad[0].pt_Y = gridVertices[indices[i]].y;
		quad[1].pt_X = gridVertices[indices[i+1]].x+smallOffset; quad[1].pt_Y = gridVertices[indices[i+1]].y;
		quad[2].pt_X = gridVertices[indices[i+2]].x+smallOffset; quad[2].pt_Y = gridVertices[indices[i+2]].y+smallOffset;
		quad[3].pt_X = gridVertices[indices[i+3]].x; quad[3].pt_Y = gridVertices[indices[i+3]].y+smallOffset;

		if (!((quad[0].pt_X == quad[1].pt_X && quad[0].pt_Y == quad[1].pt_Y) || (quad[0].pt_X == quad[3].pt_X && quad[0].pt_Y == quad[3].pt_Y)))
			MapCel(&gridMesh->cel[j], quad);
		++j;
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
	if (gridVertices==NULL) {
		gridVertices = (Point2D*)AllocAPointer((gridWidth+1)*(gridHeight+1)*sizeof(Point2D));
	}

	if (gridMesh==NULL) {
		gridMesh = initGridMesh(gridWidth, gridHeight);
	}
}
