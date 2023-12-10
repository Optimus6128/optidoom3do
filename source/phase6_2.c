#include "Doom.h"
#include <IntMath.h>

#include "string.h"

#define CCB_ARRAY_WALL_MAX MAXSCREENWIDTH

static BitmapCCB CCBArrayWall[CCB_ARRAY_WALL_MAX];		// Array of CCB structs for rendering a batch of wall columns
static int CCBArrayWallCurrent = 0;
static int CCBflagsCurrentAlteredIndex = 0;

uint32* CCBflagsAlteredIndexPtr[MAXWALLCMDS];	// Array of pointers to CEL flags to set/remove LD_PLUT

static const int flatColTexWidth = 1;   //  static const int flatColTexWidthShr = 0;
static const int flatColTexHeight = 4;  static const int flatColTexHeightShr = 2;
static const int flatColTexStride = 8;
static unsigned char *texColBufferFlat = NULL;

static drawtex_t drawtex;

viscol_t viscols[MAXSCREENWIDTH];

static uint16 *coloredWallPals = NULL;
static int currentWallCount = 0;

static Word *LightTablePtr = LightTable;


/**********************************

	Calculate texturecolumn and iscale for the rendertexture routine

**********************************/

static void initCCBarrayWall(void)
{
	BitmapCCB *CCBPtr = CCBArrayWall;

	int i;
	for (i=0; i<CCB_ARRAY_WALL_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (BitmapCCB *)(sizeof(BitmapCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
                            CCB_ACE|CCB_BGND|/*CCB_NOBLK|*/CCB_PPABS|CCB_ACSC|CCB_ALSC|CCB_PLUTPOS;	// ccb_flags

        CCBPtr->ccb_HDX = 0<<20;
        CCBPtr->ccb_VDX = 1<<16;
        CCBPtr->ccb_VDY = 0<<16;

		++CCBPtr;
	}
}

static void initCCBarrayWallFlat(void)
{
	BitmapCCB *CCBPtr;
	int i;
	//int x,y;
	Word pre0, pre1;

	if (!texColBufferFlat) {
		const int flatColTextSize = flatColTexStride * flatColTexHeight;
		texColBufferFlat = (unsigned char*)AllocAPointer(flatColTextSize * sizeof(unsigned char));
		memset(texColBufferFlat, 0, flatColTextSize);
	}

    pre0 = 0x00000001 | ((flatColTexHeight - 1) << 6);
    pre1 = (((flatColTexStride >> 2) - 2) << 24) | (flatColTexWidth - 1);

	CCBPtr = CCBArrayWall;
	for (i=0; i<CCB_ARRAY_WALL_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (BitmapCCB *)(sizeof(BitmapCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|/*CCB_NOBLK|*/CCB_ACSC|CCB_ALSC|CCB_SPABS|CCB_PPABS|CCB_PLUTPOS;

        CCBPtr->ccb_PRE0 = pre0;
        CCBPtr->ccb_PRE1 = pre1;
        CCBPtr->ccb_SourcePtr = (CelData*)texColBufferFlat;
        CCBPtr->ccb_VDX = 0<<16;
        CCBPtr->ccb_HDX = 1<<20;
        CCBPtr->ccb_HDY = 0<<20;

		++CCBPtr;
	}
}

void initWallCELs()
{
	if (!coloredWallPals) {
		coloredWallPals = (uint16*)AllocAPointer(16 * MAXWALLCMDS * sizeof(uint16));
	}

	if (optGraphics->wallQuality == WALL_QUALITY_LO) {
		initCCBarrayWallFlat();
	} else {
		initCCBarrayWall();
	}
}

void drawCCBarrayWall(Word xEnd)
{
    BitmapCCB *columnCCBstart, *columnCCBend;

	columnCCBstart = &CCBArrayWall[0];				// First column CEL of the wall segment
	columnCCBend = &CCBArrayWall[xEnd];				// Last column CEL of the wall segment
	dummyCCB->ccb_NextPtr = (CCB*)columnCCBstart;	// Start with dummy to reset HDDX and HDDY

	columnCCBend->ccb_Flags |= CCB_LAST;	// Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,dummyCCB);			// Draw all the cels of a single wall in one shot
    columnCCBend->ccb_Flags ^= CCB_LAST;	// remember to flip off that CCB_LAST flag, since we don't reinit the flags for all columns every time
}

void flushCCBarrayWall()
{
	if (CCBArrayWallCurrent != 0) {
		int i;
		drawCCBarrayWall(CCBArrayWallCurrent - 1);
		CCBArrayWallCurrent = 0;

		for (i=0; i<CCBflagsCurrentAlteredIndex; ++i) {
			*CCBflagsAlteredIndexPtr[i] &= ~CCB_LDPLUT;
		}
		CCBflagsCurrentAlteredIndex = 0;
	}
}

static void PrepWallSegmentTexCol(drawtex_t *tex)
{
    int xPos = tex->xStart;
	const int xEnd = tex->xEnd;
	const Word texWidth = tex->width - 1;
	const Word texHeight = tex->height;

	Word run;
	Word colnum;	// Column in the texture
	LongWord frac;
    Word colnumOffset = 0;
	viscol_t *vc;

	BitmapCCB *CCBPtr;
	Word colnum7;
	int pre0, pre1;

	const Byte *texBitmap = &tex->data[32];

	run = (tex->topheight-tex->bottomheight)>>HEIGHTBITS;	// Source image height
	if ((int)run<=0) {		// Invalid?
		return;
	}

	frac = tex->texturemid - (tex->topheight<<FIXEDTOHEIGHT);	// Get the anchor point
	frac >>= FRACBITS;
	while (frac&0x8000) {
		--colnumOffset;
		frac += texHeight;		// Make sure it's on the shape
	}
	frac&=0x7f;		// Zap unneeded bits
	colnum7 = frac & 7;	// Get the pixel skip

    pre0 = (colnum7<<24) | 0x03;
    pre1 = 0x3E005000 | (colnum7+run-1);	// Project the pixels

    CCBPtr = &CCBArrayWall[CCBArrayWallCurrent];
    vc = viscols;
    do {
        colnum = vc->column + colnumOffset;	// Get the starting column offset
        colnum &= texWidth;		// Wrap around the texture
        colnum = (colnum*texHeight)+frac;	// Index to the shape
        colnum >>= 1;           // Pixel to byte offset
        colnum &= ~3;			// Long word align the source

        CCBPtr->ccb_PRE0 = pre0;
        CCBPtr->ccb_PRE1 = pre1;
        CCBPtr->ccb_SourcePtr = (CelData*)&texBitmap[colnum];	// Get the source ptr

        CCBPtr++;
        vc++;
    }while (++xPos <= xEnd);
}

static void DrawWallSegment(drawtex_t *tex, void *texPal, Word screenCenterY)
{
    int xPos = tex->xStart;
	const int xEnd = tex->xEnd;
	const int texTopHeight = tex->topheight;
	int top;
	viscol_t *vc;

	BitmapCCB *CCBPtr;
	int numCels;

	if (xPos > xEnd) return;
    numCels = xEnd - xPos + 1;
	if (CCBArrayWallCurrent + numCels > CCB_ARRAY_WALL_MAX) {
		flushCCBarrayWall();
	}

    CCBPtr = &CCBArrayWall[CCBArrayWallCurrent];
	CCBPtr->ccb_Flags |= CCB_LDPLUT;
	CCBPtr->ccb_PLUTPtr = texPal;
	CCBflagsAlteredIndexPtr[CCBflagsCurrentAlteredIndex++] = &CCBPtr->ccb_Flags;
    vc = viscols;
    do {
        top = screenCenterY - ((vc->scale*texTopHeight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y

        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top << 16) | 0xFF00;	// This obviously does positioning and by removing the 0xFF00 subpixel part there were little gaps with dirty pixels between floors 
													// Initially it would also bypass the custom CLUT for some reason and use the fixed (now avoided by setting CCB_PLUTPOS on the flags instead)
        CCBPtr->ccb_HDY = vc->scale<<(20-SCALEBITS);
        CCBPtr->ccb_PIXC = vc->light;// | 0x0080; // alpha test for overdrawing		// PIXC control

        CCBPtr++;
        vc++;
    }while (++xPos <= xEnd);

	PrepWallSegmentTexCol(tex);

    CCBArrayWallCurrent += numCels;
}

static void DrawWallSegmentFlat(drawtex_t *tex, const void *color, Word screenCenterY)
{
    int xPos = tex->xStart;
	const int xEnd = tex->xEnd;
	const int texTopHeight = tex->topheight;
	Word run;
	viscol_t *vc;

	BitmapCCB *CCBPtr;
	int numCels;

	if (xPos > xEnd) return;
	numCels = xEnd - xPos + 1;
	if (CCBArrayWallCurrent + numCels > CCB_ARRAY_WALL_MAX) {
		flushCCBarrayWall();
	}

	run = (texTopHeight-tex->bottomheight) >> HEIGHTBITS;	// Source image height
	if ((int)run<=0) {		// Invalid?
		return;
	}

    CCBPtr = &CCBArrayWall[CCBArrayWallCurrent];
	CCBPtr->ccb_Flags |= CCB_LDPLUT;
	CCBPtr->ccb_PLUTPtr = (void*)color;
	CCBflagsAlteredIndexPtr[CCBflagsCurrentAlteredIndex++] = &CCBPtr->ccb_Flags;
    vc = viscols;
    do {
        const int top = screenCenterY - ((vc->scale*texTopHeight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y

        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top << 16) | 0xFF00;
        CCBPtr->ccb_VDY = (run * vc->scale) << (16-flatColTexHeightShr-SCALEBITS);
        CCBPtr->ccb_PIXC = vc->light;		// PIXC control

        CCBPtr++;
        vc++;
    }while (++xPos <= xEnd);
    CCBArrayWallCurrent += numCels;
}


/**********************************

	Draw a single wall texture.
	Also save states for pending ceiling, floor and future clipping

**********************************/

static void DrawSegAny(viswall_t *segl, bool isTop, bool isFlat)
{
    texture_t *tex;
	void *texPal = NULL;

    if (isTop) {
        tex = segl->t_texture;

        drawtex.topheight = segl->t_topheight;
        drawtex.bottomheight = segl->t_bottomheight;
        drawtex.texturemid = segl->t_texturemid;
    } else {
        tex = segl->b_texture;

        drawtex.topheight = segl->b_topheight;
        drawtex.bottomheight = segl->b_bottomheight;
        drawtex.texturemid = segl->b_texturemid;
    }

	if (segl->special & SEC_SPEC_FOG) {
		LightTablePtr = LightTableFog;
	} else {
		LightTablePtr = LightTable;
	}

	if (segl->color!=0) {
		texPal = &coloredWallPals[currentWallCount << 4];
		if (++currentWallCount == MAXWALLCMDS) currentWallCount = 0;
	}

    if (isFlat) {
		if (segl->color==0) {
			texPal = (void*)&tex->color;
		} else {
			initColoredPals((uint16*)&tex->color, texPal, 1, segl->color);
		}
		DrawWallSegmentFlat(&drawtex, texPal, CenterY);
    } else {
		drawtex.width = tex->width;
		drawtex.height = tex->height;
		drawtex.data = (Byte *)*tex->data;

		if (segl->color==0) {
			texPal = drawtex.data;
		} else {
			initColoredPals((uint16*)drawtex.data, texPal, 16, segl->color);
		}

		DrawWallSegment(&drawtex, texPal, CenterY);
    }
}

void DrawSeg(viswall_t *segl, ColumnStore *columnStoreData)
{
	viscol_t *viscol;

	Word xPos = segl->LeftX;
	const Word rightX = segl->RightX;

	const Fixed offset = segl->offset;
	const angle_t centerAngle = segl->CenterAngle;
	const Word distance = segl->distance >> VISWALL_DISTANCE_PRESHIFT;

	if (!(segl->WallActions & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    drawtex.xStart = xPos;
    drawtex.xEnd = rightX;

    viscol = viscols;
    do {
        viscol->column = (offset-((finetangent[(centerAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT] * distance)>>(FRACBITS-VISWALL_DISTANCE_PRESHIFT)))>>FRACBITS;
        viscol->light = LightTablePtr[columnStoreData->light>>LIGHTSCALESHIFT];
        viscol->scale = columnStoreData->scale;
        viscol++;
		columnStoreData++;
    } while (++xPos <= rightX);

    if (segl->WallActions & AC_TOPTEXTURE)
        DrawSegAny(segl, true, false);

    if (segl->WallActions & AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, false);
}

void DrawSegFlat(viswall_t *segl, ColumnStore *columnStoreData)
{
	viscol_t *viscol;

	Word xPos = segl->LeftX;
	const Word rightX = segl->RightX;

	if (!(segl->WallActions & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = rightX;

    viscol = viscols;
    do {
        viscol->light = LightTablePtr[columnStoreData->light>>LIGHTSCALESHIFT];
        viscol->scale = columnStoreData->scale;
        viscol++;
		columnStoreData++;
    } while (++xPos <= rightX);

    if (segl->WallActions & AC_TOPTEXTURE)
        DrawSegAny(segl, true, true);

    if (segl->WallActions & AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, true);
}
