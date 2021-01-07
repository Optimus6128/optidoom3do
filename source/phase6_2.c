#include "Doom.h"
#include <IntMath.h>

#include "string.h"

#define CCB_ARRAY_WALL_MAX MAXSCREENWIDTH

static MyCCB CCBArrayWall[CCB_ARRAY_WALL_MAX];		// Array of CCB structs for rendering a batch of wall columns
static int CCBArrayWallCurrent = 0;

static const int flatColTexWidth = 1;   //  static const int flatColTexWidthShr = 0;
static const int flatColTexHeight = 8;  static const int flatColTexHeightShr = 3;
static const int flatColTexStride = 8;
static unsigned char *texColBufferFlat = NULL;

static drawtex_t drawtex;

viscol_t viscols[MAXSCREENWIDTH];

static uint16 *coloredWallPals = NULL;
static int currentWallCount = 0;


/**********************************

	Calculate texturecolumn and iscale for the rendertexture routine

**********************************/

static void initCCBarrayWall(void)
{
	MyCCB *CCBPtr = CCBArrayWall;

	int i;
	for (i=0; i<CCB_ARRAY_WALL_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDPLUT|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
                            CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_ACSC|CCB_ALSC;	// ccb_flags

        if (i==0) CCBPtr->ccb_Flags |= CCB_LDPLUT;  // First CEL column will set the palette for the rest

        CCBPtr->ccb_HDX = 0<<20;
        CCBPtr->ccb_VDX = 1<<16;
        CCBPtr->ccb_VDY = 0<<16;
		CCBPtr->ccb_HDDX = 0;
		CCBPtr->ccb_HDDY = 0;

		++CCBPtr;
	}
}

static void initCCBarrayWallFlat(void)
{
	MyCCB *CCBPtr;
	int i;
	//int x,y;
	Word pre0, pre1;

	if (!texColBufferFlat) {
		const int flatColTextSize = flatColTexStride * flatColTexHeight;
		texColBufferFlat = (unsigned char*)AllocAPointer(flatColTextSize * sizeof(unsigned char));
		memset(texColBufferFlat, 0, flatColTextSize);
	}

    pre0 = 0x00000005 | ((flatColTexHeight - 1) << 6);
    pre1 = (((flatColTexStride >> 2) - 2) << 16) | (flatColTexWidth - 1);

	CCBPtr = CCBArrayWall;
	for (i=0; i<CCB_ARRAY_WALL_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPLUT|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_ACSC|CCB_ALSC|CCB_SPABS|CCB_PPABS;

        if (i==0) CCBPtr->ccb_Flags |= CCB_LDPLUT;  // First CEL column will set the palette for the rest

        CCBPtr->ccb_PRE0 = pre0;
        CCBPtr->ccb_PRE1 = pre1;
        CCBPtr->ccb_SourcePtr = (CelData*)texColBufferFlat;
        CCBPtr->ccb_VDX = 0<<16;
        CCBPtr->ccb_HDX = 1<<20;
        CCBPtr->ccb_HDY = 0<<20;
		CCBPtr->ccb_HDDX = 0;
		CCBPtr->ccb_HDDY = 0;

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
    MyCCB *columnCCBstart, *columnCCBend;

	columnCCBstart = &CCBArrayWall[0];           // First column CEL of the wall segment
	columnCCBend = &CCBArrayWall[xEnd];          // Last column CEL of the wall segment

	columnCCBend->ccb_Flags |= CCB_LAST;         // Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,(CCB*)columnCCBstart);    // Draw all the cels of a single wall in one shot
    columnCCBend->ccb_Flags ^= CCB_LAST;         // remember to flip off that CCB_LAST flag, since we don't reinit the flags for all columns every time
}

void flushCCBarrayWall()
{
	if (CCBArrayWallCurrent != 0) {
		drawCCBarrayWall(CCBArrayWallCurrent - 1);
		CCBArrayWallCurrent = 0;
	}
}

static void DrawWallSegment(drawtex_t *tex, void *texPal, Word screenCenterY)
{
    int xPos = tex->xStart;
	int top;
	Word run;
	Word colnum;	// Column in the texture
	LongWord frac;
    Word colnumOffset = 0;
	viscol_t *vc;

	MyCCB *CCBPtr;
	Word colnum7;
	int pre0, pre1;
	int numCels;

	const Byte *texBitmap = &tex->data[32];

	if (xPos > tex->xEnd) return;
    numCels = tex->xEnd - xPos + 1;
	if (CCBArrayWallCurrent + numCels > CCB_ARRAY_WALL_MAX) {
		flushCCBarrayWall();
	}

	run = (tex->topheight-tex->bottomheight)>>HEIGHTBITS;	// Source image height
	if ((int)run<=0) {		// Invalid?
		return;
	}

	frac = tex->texturemid - (tex->topheight<<FIXEDTOHEIGHT);	// Get the anchor point
	frac >>= FRACBITS;
	while (frac&0x8000) {
		--colnumOffset;
		frac += tex->height;		// Make sure it's on the shape
	}
	frac&=0x7f;		// Zap unneeded bits
	colnum7 = frac & 7;	// Get the pixel skip

    pre0 = (colnum7<<24) | 0x03;
    pre1 = 0x3E005000 | (colnum7+run-1);	// Project the pixels

    CCBPtr = &CCBArrayWall[CCBArrayWallCurrent];
    vc = viscols;
    do {
        top = screenCenterY - ((vc->scale*tex->topheight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y
        colnum = vc->column + colnumOffset;	// Get the starting column offset
        colnum &= (tex->width-1);		// Wrap around the texture
        colnum = (colnum*tex->height)+frac;	// Index to the shape
        colnum >>= 1;           // Pixel to byte offset
        colnum &= ~3;			// Long word align the source

        CCBPtr->ccb_PRE0 = pre0;
        CCBPtr->ccb_PRE1 = pre1;
        CCBPtr->ccb_SourcePtr = (CelData*)&texBitmap[colnum];	// Get the source ptr
        CCBPtr->ccb_PLUTPtr = texPal;
        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top<<16) + 0xFF00;
        CCBPtr->ccb_HDY = vc->scale<<(20-SCALEBITS);
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control

        CCBPtr++;
        vc++;
    }while (++xPos <= tex->xEnd);
    CCBArrayWallCurrent += numCels;
}

static void DrawWallSegmentFlat(drawtex_t *tex, const void *color, Word screenCenterY)
{
    int xPos = tex->xStart;
	int top;
	Word run;
	viscol_t *vc;

	MyCCB *CCBPtr;
	int numCels;

	if (xPos > tex->xEnd) return;
	numCels = tex->xEnd - xPos + 1;
	if (CCBArrayWallCurrent + numCels > CCB_ARRAY_WALL_MAX) {
		flushCCBarrayWall();
	}

	run = (tex->topheight-tex->bottomheight) >> HEIGHTBITS;	// Source image height
	if ((int)run<=0) {		// Invalid?
		return;
	}

    CCBPtr = &CCBArrayWall[CCBArrayWallCurrent];
    vc = viscols;
    do {
        top = screenCenterY - ((vc->scale*tex->topheight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y

        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top<<16) + 0xFF00;
        CCBPtr->ccb_VDY = (run * vc->scale) << (16-flatColTexHeightShr-SCALEBITS);
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control
        CCBPtr->ccb_PLUTPtr = (void*)color;

        CCBPtr++;
        vc++;
    }while (++xPos <= tex->xEnd);
    CCBArrayWallCurrent += numCels;
}


/**********************************

	Draw a single wall texture.
	Also save states for pending ceiling, floor and future clipping

**********************************/

static void DrawSegAny(viswall_t *segl, bool isTop, bool isFlat)
{
    texture_t *tex;

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

    if (isFlat) {
        DrawWallSegmentFlat(&drawtex, &tex->color, CenterY);
    } else {
		void *texPal;

		drawtex.width = tex->width;
		drawtex.height = tex->height;
		drawtex.data = (Byte *)*tex->data;

		if (segl->color==0) {
			texPal = drawtex.data;
		} else {
			texPal = &coloredWallPals[currentWallCount << 4];
			initColoredPals((uint16*)drawtex.data, texPal, 16, segl->color);
		    if (++currentWallCount == MAXWALLCMDS) currentWallCount = 0;
		}

		DrawWallSegment(&drawtex, texPal, CenterY);
    }
}

void DrawSeg(viswall_t *segl, int *scaleData)
{
    Word i;

	int scale;

	int textureColumn;
	int textureLight;

	viscol_t *viscol;

	Word xPos = segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;


    i = segl->seglightlevel;
    lightmin = lightmins[i];
    lightmax = i;
    lightsub = lightsubs[i];
    lightcoef = lightcoefs[i];

    viscol = viscols;
    do {
        scale = *scaleData++;

        /*textureColumn = (segl->offset-IMFixMul(
            finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT],
            segl->distance))>>FRACBITS;*/

		// I'll keep the above commented for now as I need to test that this replacement of IMFixMul with regular mul doesn't overflow in any level.
        textureColumn = (segl->offset-((finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT] * segl->distance)>>(FRACBITS-VISWALL_DISTANCE_PRESHIFT)))>>FRACBITS;

        textureLight = ((scale*lightcoef)>>16) - lightsub;
        if (textureLight < lightmin) {
            textureLight = lightmin;
        }
        if (textureLight > lightmax) {
            textureLight = lightmax;
        }

        viscol->column = textureColumn;
        viscol->light = textureLight;
        viscol->scale = scale;
        viscol++;

        ++xPos;
    } while (xPos <= segl->RightX);


    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAny(segl, true, false);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, false);
}

void DrawSegUnshaded(viswall_t *segl, int *scaleData)
{
	int scale;

	int textureColumn;
	int textureLight;

	viscol_t *viscol;

	Word xPos = segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    textureLight = segl->seglightlevel;
    if (optGraphics->depthShading == DEPTH_SHADING_DARK) textureLight = lightmins[textureLight];

    viscol = viscols;
    do {
        scale = *scaleData++;

        /*textureColumn = (segl->offset-IMFixMul(
            finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT],
            segl->distance))>>FRACBITS;*/

		// I'll keep the above commented for now as I need to test that this replacement of IMFixMul with regular mul doesn't overflow in any level.
		textureColumn = (segl->offset-((finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT] * segl->distance)>>(FRACBITS-VISWALL_DISTANCE_PRESHIFT)))>>FRACBITS;

        viscol->column = textureColumn;
        viscol->light = textureLight;
        viscol->scale = scale;
        viscol++;

        ++xPos;
    } while (xPos <= segl->RightX);


    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAny(segl, true, false);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, false);
}

void DrawSegFlat(viswall_t *segl, int *scaleData)
{
    Word i;

	int scale;

	int textureLight;

	viscol_t *viscol;

	Word xPos = segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;


    i = segl->seglightlevel;
    lightmin = lightmins[i];
    lightmax = i;
    lightsub = lightsubs[i];
    lightcoef = lightcoefs[i];

    viscol = viscols;
    do {
        scale = *scaleData++;

        textureLight = ((scale*lightcoef)>>16) - lightsub;
        if (textureLight < lightmin) {
            textureLight = lightmin;
        }
        if (textureLight > lightmax) {
            textureLight = lightmax;
        }

        viscol->light = textureLight;
        viscol->scale = scale;
        viscol++;

        ++xPos;
    } while (xPos <= segl->RightX);


    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAny(segl, true, true);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, true);
}

void DrawSegFlatUnshaded(viswall_t *segl, int *scaleData)
{
	int textureLight;

	viscol_t *viscol;

	Word xPos = segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    textureLight = segl->seglightlevel;
    if (optGraphics->depthShading == DEPTH_SHADING_DARK) textureLight = lightmins[textureLight];

    viscol = viscols;
    do {
        viscol->light = textureLight;
        viscol->scale = *scaleData++;
        viscol++;

        ++xPos;
    } while (xPos <= segl->RightX);


    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAny(segl, true, true);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, true);
}
