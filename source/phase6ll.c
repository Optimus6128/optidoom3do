#include "Doom.h"
#include <IntMath.h>

#include "string.h"

static drawtex_t drawtex;

static int scaleLeft, scaleRight;
static Word light;

static MyCCB CCBQuadWallFlat, CCBQuadWallTextured;

static const int flatTexWidth = 8;  static const int flatTexWidthShr = 3;
static const int flatTexHeight = 1;  static const int flatTexHeightShr = 0;
static const int flatTexStride = 8;
static unsigned char *texBufferFlat;

static const int mode8bpp = 5;
static const int mode4bpp = 3;

#define RECIPROCAL_X_MAX_NUM 129
#define RECIPROCAL_Y_MAX_NUM 512
#define RECIPROCAL_X_FP 16
#define RECIPROCAL_Y_FP 16
static Word reciprocalLengthX[RECIPROCAL_X_MAX_NUM];
static Word reciprocalLengthY[RECIPROCAL_Y_MAX_NUM];


void initCCBQuadWallFlat()
{
    const int flatTexSize = flatTexStride * flatTexHeight;
    texBufferFlat = (unsigned char*)malloc(flatTexSize * sizeof(unsigned char));
    memset(texBufferFlat, 0, flatTexSize);

    CCBQuadWallFlat.ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV|CCB_ACSC|CCB_ALSC|CCB_LAST;
    CCBQuadWallFlat.ccb_PRE0 = mode8bpp | ((flatTexHeight - 1) << 6);
    CCBQuadWallFlat.ccb_PRE1 = (((flatTexStride >> 2) - 2) << 16) | (flatTexWidth - 1);

    CCBQuadWallFlat.ccb_SourcePtr = (CelData*)texBufferFlat;
}

void initCCBQuadWallTextured()
{
    int i;

    CCBQuadWallTextured.ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV|CCB_ACSC|CCB_ALSC|CCB_LAST;

    for (i=0; i<RECIPROCAL_X_MAX_NUM; ++i) {
        reciprocalLengthX[i] = (1 << RECIPROCAL_X_FP) / i;
    }

    for (i=0; i<RECIPROCAL_Y_MAX_NUM; ++i) {
        reciprocalLengthY[i] = (1 << RECIPROCAL_Y_FP) / i;
    }
}

static void DrawWallSegmentFlatLL(drawtex_t *tex, Word screenCenterY)
{
    const Word xLeft = tex->xStart;
    const Word xRight = tex->xEnd;

	int topLeft, topRight;
	int bottomLeft, bottomRight;

    Word run = (tex->topheight - tex->bottomheight) >> HEIGHTBITS;

	if (xLeft > xRight) return;


	topLeft = screenCenterY - ((scaleLeft * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
	topRight = screenCenterY - ((scaleRight * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
	bottomLeft = topLeft + ((run * scaleLeft) >> SCALEBITS) + 1;
	bottomRight = topRight + ((run * scaleRight) >> SCALEBITS) + 1;


	if (opt_extraRender != EXTRA_RENDER_WIREFRAME) {
        const int lengthLeft = bottomLeft - topLeft + 1;
        const int lengthRight = bottomRight - topRight + 1;
        const int lengthDiff = lengthRight - lengthLeft;


        CCBQuadWallFlat.ccb_XPos = xLeft << 16;
        CCBQuadWallFlat.ccb_YPos = bottomLeft << 16;
        CCBQuadWallFlat.ccb_VDX = (xRight - xLeft + 1) << (16 - flatTexHeightShr);
        CCBQuadWallFlat.ccb_VDY = (bottomRight - bottomLeft) << (16 - flatTexHeightShr);

        CCBQuadWallFlat.ccb_HDX = 0;
        CCBQuadWallFlat.ccb_HDY = -lengthLeft << (20 - flatTexWidthShr);

        CCBQuadWallFlat.ccb_HDDX = 0;
        CCBQuadWallFlat.ccb_HDDY = -lengthDiff << (20 - flatTexWidthShr - flatTexHeightShr);


        CCBQuadWallFlat.ccb_PLUTPtr = tex->data;
        CCBQuadWallFlat.ccb_PIXC = LightTable[light>>LIGHTSCALESHIFT];

        DrawCels(VideoItem,(CCB*)&CCBQuadWallFlat);
	} else {
	    const Word color = *((Word*)tex->data);

	    const int x0 = xLeft - (ScreenWidth >> 1);
	    const int x1 = xRight - (ScreenWidth >> 1);

	    topLeft = ScreenHeight - topLeft - (ScreenHeight >> 1);
	    topRight = ScreenHeight - topRight - (ScreenHeight >> 1);
	    bottomLeft = ScreenHeight - bottomLeft - (ScreenHeight >> 1);
	    bottomRight = ScreenHeight - bottomRight - (ScreenHeight >> 1);

        DrawThickLine(x0, topLeft, x1, topRight, color);
        DrawThickLine(x1, topRight, x1, bottomRight, color);
        DrawThickLine(x1, bottomRight, x0, bottomLeft, color);
        DrawThickLine(x0, bottomLeft, x0, topLeft, color);
	}
}


static void DrawWallSegmentTexturedLL(drawtex_t *tex, Word screenCenterY)
{
    const Word xLeft = tex->xStart;
    const Word xRight = tex->xEnd;

    const Word texWidth = tex->width;
    const Word recWidth = reciprocalLengthX[texWidth];
    Word recHeight;

    const Word run = (tex->topheight - tex->bottomheight) >> HEIGHTBITS;
    LongWord frac;
    Word colnumOffset = 0;
    Word colnum;
    Word colnum7;

    Byte *texData = tex->data;

	int topLeft, topRight;
	int bottomLeft, bottomRight;
    int lengthLeft, lengthRight, lengthDiff;

    Word texStride = tex->height >> 1;
    if (texStride < 8) texStride = 8;

	if (xLeft > xRight) return;


	        //textureColumn = (segl->offset-IMFixMul(
            //finetangent[(segl->CenterAngle+xtoviewangle[xPos])>>ANGLETOFINESHIFT],
            //segl->distance))>>FRACBITS;


    frac = tex->texturemid - (tex->topheight<<FIXEDTOHEIGHT);	// Get the anchor point
	frac >>= FRACBITS;
	while (frac&0x8000) {
        //--colnumOffset;
		frac += tex->height;		// Make sure it's on the shape
	}
	frac&=0x7f;		// Zap unneeded bits

        colnum = /*textureColumn +*/ colnumOffset;	// Get the starting column offset
        colnum &= (tex->width-1);		// Wrap around the texture
        colnum = (colnum*tex->height)+frac;	// Index to the shape

        colnum7 = colnum & 7;	// Get the pixel skip
        colnum >>= 1;           // Pixel to byte offset
        colnum += 32;			// Index past the PLUT
        colnum &= ~3;			// Long word align the source

	recHeight = reciprocalLengthY[run];

	topLeft = screenCenterY - ((scaleLeft * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
	topRight = screenCenterY - ((scaleRight * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
	bottomLeft = topLeft + ((run * scaleLeft) >> SCALEBITS) + 1;
	bottomRight = topRight + ((run * scaleRight) >> SCALEBITS) + 1;

    lengthLeft = bottomLeft - topLeft + 1;
    lengthRight = bottomRight - topRight + 1;
    lengthDiff = lengthRight - lengthLeft;

    CCBQuadWallTextured.ccb_XPos = xLeft << 16;
    CCBQuadWallTextured.ccb_YPos = topLeft << 16;
    CCBQuadWallTextured.ccb_VDX = ((xRight - xLeft + 1) * recWidth) << (16 - RECIPROCAL_X_FP);
    CCBQuadWallTextured.ccb_VDY = ((topRight - topLeft) * recWidth) << (16 - RECIPROCAL_X_FP);

    CCBQuadWallTextured.ccb_HDX = 0;
    CCBQuadWallTextured.ccb_HDY = (lengthLeft * recHeight) << (20 - RECIPROCAL_Y_FP);

    CCBQuadWallTextured.ccb_HDDX = 0;
    CCBQuadWallTextured.ccb_HDDY = (int)((lengthDiff * recWidth) << (20 - RECIPROCAL_X_FP)) / (int)run;


    CCBQuadWallTextured.ccb_PLUTPtr = texData;
    CCBQuadWallTextured.ccb_SourcePtr = (CelData*)(texData + colnum);
    CCBQuadWallTextured.ccb_PIXC = LightTable[light>>LIGHTSCALESHIFT];

    CCBQuadWallTextured.ccb_PRE0 = (colnum7 << 24) | mode4bpp | ((texWidth - 1) << 6);
    CCBQuadWallTextured.ccb_PRE1 = (((texStride >> 2) - 2) << 24) | (colnum7 + run - 1);

    DrawCels(VideoItem,(CCB*)&CCBQuadWallTextured);
}


/**********************************

	Draw a single wall texture.
	Also save states for pending ceiling, floor and future clipping

**********************************/

static void DrawSegAnyLL(viswall_t *segl, bool isTop, bool isTextured)
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

    if (isTextured) {
        DrawWallSegmentTexturedLL(&drawtex, CenterY);
    } else {
        DrawWallSegmentFlatLL(&drawtex, CenterY);
    }
}

void DrawSegUnshadedLL(viswall_t *segl, int *scaleData, bool isTextured)
{
	const Word xLength = segl->RightX - segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    light = segl->seglightlevel;
    if (opt_depthShading == DEPTH_SHADING_DARK) light = lightmins[light];

    scaleLeft = *scaleData;
    scaleRight = *(scaleData + xLength);

    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAnyLL(segl, true, isTextured);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAnyLL(segl, false, isTextured);
}
