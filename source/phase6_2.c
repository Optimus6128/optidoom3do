#include "Doom.h"
#include <IntMath.h>

#include "string.h"

static MyCCB CCBArrayWall[MAXSCREENWIDTH];		// Array of CCB structs for a single wall segment
static MyCCB CCBArrayWallFlat[MAXSCREENWIDTH];	// Array of CCB structs for a single wall segment (untextured)

static const int flatColTexWidth = 1;   //  static const int flatColTexWidthShr = 0;
static const int flatColTexHeight = 8;  static const int flatColTexHeightShr = 3;
static const int flatColTexStride = 8;
static unsigned char *texColBufferFlat;

static drawtex_t drawtex;

viscol_t viscols[MAXSCREENWIDTH];


/**********************************

	Calculate texturecolumn and iscale for the rendertexture routine

**********************************/

void initCCBarrayWall(void)
{
	MyCCB *CCBPtr;
	int i;

    columnWidth = 1;
    if (opt_wallQuality == WALL_QUALITY_MED)
        columnWidth = 2;

	CCBPtr = CCBArrayWall;
	for (i=0; i<MAXSCREENWIDTH; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
                            CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_USEAV|CCB_ACSC|CCB_ALSC;	// ccb_flags

        if (i==0) CCBPtr->ccb_Flags |= CCB_LDPLUT;  // First CEL column will set the palette for the rest

        CCBPtr->ccb_HDX = 0<<20;
        CCBPtr->ccb_VDX = columnWidth<<16;
        CCBPtr->ccb_VDY = 0<<16;
		CCBPtr->ccb_HDDX = 0;
		CCBPtr->ccb_HDDY = 0;

		++CCBPtr;
	}
}

void initCCBarrayWallFlat(void)
{
	MyCCB *CCBPtr;
	int i;
	//int x,y;
	Word pre0, pre1;

    const int flatColTextSize = flatColTexStride * flatColTexHeight;
    texColBufferFlat = (unsigned char*)malloc(flatColTextSize * sizeof(unsigned char));
    memset(texColBufferFlat, 0, flatColTextSize);


    /*
    i = 0;
    for (y=0; y<flatColTexHeight; ++y) {
        for (x=0; x<flatColTexStride; ++x) {
            texColBufferFlat[i++] = y;
        }
    }*/


    pre0 = 0x00000005 | ((flatColTexHeight - 1) << 6);
    pre1 = (((flatColTexStride >> 2) - 2) << 16) | (flatColTexWidth - 1);

	CCBPtr = CCBArrayWallFlat;
	for (i=0; i<MAXSCREENWIDTH; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_USEAV|CCB_ACSC|CCB_ALSC|CCB_SPABS|CCB_PPABS|CCB_LDPLUT;

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


void drawCCBarray(MyCCB* lastCCB, MyCCB *CCBArrayPtr)
{
    MyCCB *columnCCBstart, *columnCCBend;

	columnCCBstart = CCBArrayPtr;                // First column CEL of the wall segment
	columnCCBend = lastCCB;                      // Last column CEL of the wall segment

	columnCCBend->ccb_Flags |= CCB_LAST;         // Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,(CCB*)columnCCBstart);    // Draw all the cels of a single wall in one shot
    columnCCBend->ccb_Flags ^= CCB_LAST;         // remember to flip off that CCB_LAST flag, since we don't reinit the flags for all columns every time
}

static void DrawWallSegmentFull(drawtex_t *tex, Word screenCenterY)
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

	if (xPos > tex->xEnd) return;

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

    CCBPtr = CCBArrayWall;
    vc = viscols;
    do {
        top = screenCenterY - ((vc->scale*tex->topheight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y
        colnum = vc->column + colnumOffset;	// Get the starting column offset
        colnum &= (tex->width-1);		// Wrap around the texture
        colnum = (colnum*tex->height)+frac;	// Index to the shape

        colnum7 = colnum & 7;	// Get the pixel skip
        colnum >>= 1;           // Pixel to byte offset
        colnum += 32;			// Index past the PLUT
        colnum &= ~3;			// Long word align the source

        CCBPtr->ccb_PRE0 = (colnum7<<24) | 0x03;
        CCBPtr->ccb_PRE1 = 0x3E005000 | (colnum7+run-1);	// Project the pixels
        CCBPtr->ccb_SourcePtr = (CelData*)&tex->data[colnum];	// Get the source ptr
        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top<<16) + 0xFF00;
        CCBPtr->ccb_HDY = vc->scale<<(20-SCALEBITS);
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control

        CCBPtr++;
        vc++;
    }while (++xPos <= tex->xEnd);

    // Call for the final render of the linked list of all the column cels of the single wall segment
    CCBArrayWall[0].ccb_PLUTPtr = tex->data;    // plut pointer only for first element
    drawCCBarray(--CCBPtr, CCBArrayWall);
}

static void DrawWallSegmentHalf(drawtex_t *tex, Word screenCenterY)
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

	if (xPos > tex->xEnd) return;

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

    CCBPtr = CCBArrayWall;
    vc = viscols;
    do {
        top = screenCenterY - ((vc->scale*tex->topheight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y
        colnum = vc->column + colnumOffset;	// Get the starting column offset
        colnum &= (tex->width-1);		// Wrap around the texture
        colnum = (colnum*tex->height)+frac;	// Index to the shape

        colnum7 = colnum & 7;	// Get the pixel skip
        colnum >>= 1;           // Pixel to byte offset
        colnum += 32;			// Index past the PLUT
        colnum &= ~3;			// Long word align the source

        CCBPtr->ccb_PRE0 = (colnum7<<24) | 0x03;
        CCBPtr->ccb_PRE1 = 0x3E005000 | (colnum7+run-1);	// Project the pixels
        CCBPtr->ccb_SourcePtr = (CelData*)&tex->data[colnum];	// Get the source ptr
        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top<<16) + 0xFF00;
        CCBPtr->ccb_HDY = vc->scale<<(20-SCALEBITS);
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control

        CCBPtr++;
        vc++;
        xPos+=2;
    }while (xPos <= tex->xEnd);

    // Call for the final render of the linked list of all the column cels of the single wall segment
    CCBArrayWall[0].ccb_PLUTPtr = tex->data;    // plut pointer only for first element
    drawCCBarray(--CCBPtr, CCBArrayWall);
}

static void DrawWallSegmentFlat(drawtex_t *tex, Word screenCenterY)
{
    int xPos = tex->xStart;
	int top;
	Word run;
	viscol_t *vc;

	MyCCB *CCBPtr;

	if (xPos > tex->xEnd) return;

	run = (tex->topheight-tex->bottomheight) >> HEIGHTBITS;	// Source image height
	if ((int)run<=0) {		// Invalid?
		return;
	}

    CCBPtr = CCBArrayWallFlat;
    vc = viscols;
    do {
        top = screenCenterY - ((vc->scale*tex->topheight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y

        CCBPtr->ccb_PLUTPtr = tex->data;
        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top<<16) + 0xFF00;
        CCBPtr->ccb_VDY = (run * vc->scale) << (16-flatColTexHeightShr-SCALEBITS);
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control

        CCBPtr++;
        vc++;
    }while (++xPos <= tex->xEnd);

    // Call for the final render of the linked list of all the column cels of the single wall segment
    drawCCBarray(--CCBPtr, CCBArrayWallFlat);
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
    drawtex.width = tex->width;
    drawtex.height = tex->height;
    drawtex.data = (Byte *)*tex->data;

    if (isFlat) {
        DrawWallSegmentFlat(&drawtex, CenterY);
    } else {
        if (columnWidth==1) {
            DrawWallSegmentFull(&drawtex, CenterY);
        } else {
            DrawWallSegmentHalf(&drawtex, CenterY);
        }
    }
}

void DrawSegFull(viswall_t *segl, int *scaleData)
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

        textureColumn = (segl->offset-IMFixMul(
            finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT],
            segl->distance))>>FRACBITS;

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

void DrawSegFullUnshaded(viswall_t *segl, int *scaleData)
{
	int scale;

	int textureColumn;
	int textureLight;

	viscol_t *viscol;

	Word xPos = segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    textureLight = segl->seglightlevel;
    if (opt_depthShading == DEPTH_SHADING_DARK) textureLight = lightmins[textureLight];

    viscol = viscols;
    do {
        scale = *scaleData++;

        textureColumn = (segl->offset-IMFixMul(
            finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT],
            segl->distance))>>FRACBITS;

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

void DrawSegFullFlat(viswall_t *segl, int *scaleData)
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

void DrawSegFullFlatUnshaded(viswall_t *segl, int *scaleData)
{
	int textureLight;

	viscol_t *viscol;

	Word xPos = segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    textureLight = segl->seglightlevel;
    if (opt_depthShading == DEPTH_SHADING_DARK) textureLight = lightmins[textureLight];

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

void DrawSegHalf(viswall_t *segl, int *scaleData)
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
        scale = *scaleData;
        scaleData+=2;

        textureColumn = (segl->offset-IMFixMul(
            finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT],
            segl->distance))>>FRACBITS;

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

        xPos += 2;
    } while (xPos <= segl->RightX);


    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAny(segl, true, false);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, false);
}

void DrawSegHalfUnshaded(viswall_t *segl, int *scaleData)
{
	int scale;

	int textureColumn;
	int textureLight;

	viscol_t *viscol;

	Word xPos = segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    textureLight = segl->seglightlevel;
    if (opt_depthShading == DEPTH_SHADING_DARK) textureLight = lightmins[textureLight];

    viscol = viscols;
    do {
        scale = *scaleData;
        scaleData+=2;

        textureColumn = (segl->offset-IMFixMul(
            finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT],
            segl->distance))>>FRACBITS;

        viscol->column = textureColumn;
        viscol->light = textureLight;
        viscol->scale = scale;
        viscol++;

        xPos += 2;
    } while (xPos <= segl->RightX);


    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAny(segl, true, false);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAny(segl, false, false);
}
