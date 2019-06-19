#include "Doom.h"
#include <IntMath.h>

#include "string.h"

static drawtex_t drawtex;

static int scaleLeft, scaleRight;
static Word light;

static MyCCB CCBQuadWallFlat;

static const int flatTexWidth = 8;  static const int flatTexWidthShr = 3;
static const int flatTexHeight = 1;  static const int flatTexHeightShr = 0;
static const int flatTexStride = 8;
static unsigned char *texBufferFlat;

void initCCBQuadWallFlat()
{
    //int x,y,i=0;
    const int flatTextSize = flatTexStride * flatTexHeight;
    texBufferFlat = (unsigned char*)malloc(flatTextSize * sizeof(unsigned char));
    memset(texBufferFlat, 0, flatTextSize);

    /*
    for (y=0; y<flatTexHeight; ++y) {
        for (x=0; x<flatTexStride; ++x) {
            texBufferFlat[i++] = (x^y) & 31;
        }
    }
    */

    CCBQuadWallFlat.ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV|CCB_ACSC|CCB_ALSC|CCB_LAST;
    CCBQuadWallFlat.ccb_PRE0 = 0x00000005 | ((flatTexHeight - 1) << 6);
    CCBQuadWallFlat.ccb_PRE1 = (((flatTexStride >> 2) - 2) << 16) | (flatTexWidth - 1);

    CCBQuadWallFlat.ccb_SourcePtr = (CelData*)texBufferFlat;
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


	if (extraRenderOption != 1) {
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


/**********************************

	Draw a single wall texture.
	Also save states for pending ceiling, floor and future clipping

**********************************/

static void DrawSegAnyLL(viswall_t *segl, bool isTop, bool isFlat)
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


    DrawWallSegmentFlatLL(&drawtex, CenterY);
}

void DrawSegFullFlatUnshadedLL(viswall_t *segl, int *scaleData)
{
	const Word xLength = segl->RightX - segl->LeftX;

    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    light = segl->seglightlevel;
    if (!depthShadingOption) light = lightmins[light];

    scaleLeft = *scaleData;
    scaleRight = *(scaleData + xLength);

    drawtex.xStart = segl->LeftX;
    drawtex.xEnd = segl->RightX;

    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAnyLL(segl, true, true);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAnyLL(segl, false, true);
}
