#include "Doom.h"
#include <IntMath.h>

#include "string.h"

// haven't seen more than 256, was 320 but just to make sure for now
#define MAX_WALL_PARTS 512

typedef struct {
    int xLeft, xRight;
    int scaleLeft, scaleRight;
    int textureOffset, textureLength;
    Word shadeLight;
} wallpart_t;

static wallpart_t wallparts[MAX_WALL_PARTS];
static int wallpartsCount = 0;

static drawtex_t drawtex;

Word ambientLight;

static MyCCB CCBQuadWallFlat;
static MyCCB CCBQuadWallTextured[MAX_WALL_PARTS];

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
static int reciprocalLengthX[RECIPROCAL_X_MAX_NUM];
static int reciprocalLengthY[RECIPROCAL_Y_MAX_NUM];

static bool doWireTex;

int fullWallPartCount;
int brokenWallPartCount;

void initCCBQuadWallFlat()
{
    const int flatTexSize = flatTexStride * flatTexHeight;
    texBufferFlat = (unsigned char*)malloc(flatTexSize * sizeof(unsigned char));
    memset(texBufferFlat, 0, flatTexSize);

    CCBQuadWallFlat.ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV|CCB_ACSC|CCB_ALSC|CCB_LAST;
    CCBQuadWallFlat.ccb_PRE0 = mode8bpp | ((flatTexHeight - 1) << 6);
    CCBQuadWallFlat.ccb_PRE1 = (((flatTexStride >> 2) - 2) << 16) | (flatTexWidth - 1);

    CCBQuadWallFlat.ccb_HDX = 0;
    CCBQuadWallFlat.ccb_HDDX = 0;

    CCBQuadWallFlat.ccb_SourcePtr = (CelData*)texBufferFlat;
}

void initCCBQuadWallTextured()
{
	MyCCB *CCBPtr;
	int i;

	CCBPtr = CCBQuadWallTextured;
	for (i=0; i<MAX_WALL_PARTS; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_USEAV|CCB_ACSC|CCB_ALSC;
        if (i==0) CCBPtr->ccb_Flags |= CCB_LDPLUT;  // First CEL column will set the palette and shading for the rest

        CCBPtr->ccb_HDX = 0;
        CCBPtr->ccb_HDDX = 0;

        ++CCBPtr;
	}

    for (i=0; i<RECIPROCAL_X_MAX_NUM; ++i) {
        reciprocalLengthX[i] = (1 << RECIPROCAL_X_FP) / i;
    }

    for (i=0; i<RECIPROCAL_Y_MAX_NUM; ++i) {
        reciprocalLengthY[i] = (1 << RECIPROCAL_Y_FP) / i;
    }
}

static void DrawWallSegmentFlatLL(drawtex_t *tex, Word screenCenterY)
{
	int topLeft, topRight;
	int bottomLeft, bottomRight;

    Word run = (tex->topheight - tex->bottomheight) >> HEIGHTBITS;

    wallpart_t *wt = wallparts;

    const Word xLeft = wt->xLeft;
    const Word xRight = wt->xRight;

    int scaleLeft;
    int scaleRight;

	if (xLeft > xRight) return;


	scaleLeft = wt->scaleLeft;
    scaleRight = wt->scaleRight;

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
        CCBQuadWallFlat.ccb_PIXC = LightTable[ambientLight>>LIGHTSCALESHIFT];

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
    MyCCB *CCBPtr;

    const int run = (tex->topheight - tex->bottomheight) >> HEIGHTBITS;
    int recHeight;

    wallpart_t *wt = wallparts;
    int count = wallpartsCount;

    Byte *texData = tex->data;

    Word texStride = tex->height >> 1;
    if (texStride < 8) texStride = 8;

    if (run <= 0) return;
    recHeight = reciprocalLengthY[run];

    CCBPtr = CCBQuadWallTextured;
    do {
        int topLeft, topRight;
        int bottomLeft, bottomRight;
        int lengthLeft, lengthRight, lengthDiff;

        LongWord frac;
        //Word colnumOffset = 0;
        Word colnum;
        Word colnum7;


        const int texWidth = wt->textureLength;
        const int recWidth = reciprocalLengthX[texWidth];

        const int xLeft = wt->xLeft;
        const int xRight = wt->xRight;

        const int scaleLeft = wt->scaleLeft;
        const int scaleRight = wt->scaleRight;

        const int textureOffset = wt->textureOffset;


        if (xLeft > xRight) return;


        frac = tex->texturemid - (tex->topheight<<FIXEDTOHEIGHT);	// Get the anchor point
        frac >>= FRACBITS;
        while (frac&0x8000) {
            //--colnumOffset;
            frac += tex->height;		// Make sure it's on the shape
        }
        frac&=0x7f;		// Zap unneeded bits

        colnum = textureOffset;// + colnumOffset;	// Get the starting column offset
        colnum = (colnum*tex->height)+frac;	// Index to the shape

        colnum7 = colnum & 7;	// Get the pixel skip
        colnum >>= 1;           // Pixel to byte offset
        colnum += 32;			// Index past the PLUT
        colnum &= ~3;			// Long word align the source


        topLeft = screenCenterY - ((scaleLeft * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
        topRight = screenCenterY - ((scaleRight * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
        bottomLeft = topLeft + ((run * scaleLeft) >> SCALEBITS) + 1;
        bottomRight = topRight + ((run * scaleRight) >> SCALEBITS) + 1;

        lengthLeft = bottomLeft - topLeft + 1;
        lengthRight = bottomRight - topRight + 1;
        lengthDiff = lengthRight - lengthLeft;

        CCBPtr->ccb_XPos = xLeft << 16;
        CCBPtr->ccb_YPos = topLeft << 16;
        CCBPtr->ccb_VDX = ((xRight - xLeft + 1) * recWidth) << (16 - RECIPROCAL_X_FP);
        CCBPtr->ccb_VDY = ((topRight - topLeft) * recWidth) << (16 - RECIPROCAL_X_FP);

        CCBPtr->ccb_HDY = (lengthLeft * recHeight) << (20 - RECIPROCAL_Y_FP);

        CCBPtr->ccb_HDDY = ((lengthDiff * recWidth) << (20 - RECIPROCAL_X_FP)) / run;


        CCBPtr->ccb_SourcePtr = (CelData*)(texData + colnum);
        CCBPtr->ccb_PIXC = LightTable[ambientLight>>LIGHTSCALESHIFT];

        CCBPtr->ccb_PRE0 = (colnum7 << 24) | mode4bpp | ((texWidth - 1) << 6);
        CCBPtr->ccb_PRE1 = (((texStride >> 2) - 2) << 24) | (colnum7 + run - 1);


        if (opt_extraRender == EXTRA_RENDER_WIREFRAME) {

            const int x0 = xLeft - (ScreenWidth >> 1);
            const int x1 = xRight - (ScreenWidth >> 1);

            Word color = 0x3FFF;
            if (!doWireTex) {
                color = 0x00FF;
            }

            topLeft = ScreenHeight - topLeft - (ScreenHeight >> 1);
            topRight = ScreenHeight - topRight - (ScreenHeight >> 1);
            bottomLeft = ScreenHeight - bottomLeft - (ScreenHeight >> 1);
            bottomRight = ScreenHeight - bottomRight - (ScreenHeight >> 1);

            DrawThickLine(x0, topLeft, x1, topRight, color);
            DrawThickLine(x1, topRight, x1, bottomRight, color);
            DrawThickLine(x1, bottomRight, x0, bottomLeft, color);
            DrawThickLine(x0, bottomLeft, x0, topLeft, color);
        }

        ++CCBPtr;
        ++wt;
    } while(--count != 0);

    CCBQuadWallTextured[0].ccb_PLUTPtr = texData;    // plut pointer only for first element
    drawCCBarray(--CCBPtr, CCBQuadWallTextured);
}


/**********************************

	Draw a single wall texture.
	Also save states for pending ceiling, floor and future clipping

**********************************/

static void PrepareBrokenWallparts(viswall_t *segl, Word texWidth, int *scaleData)
{
    wallpart_t *wt = wallparts;

    int x = segl->LeftX;
    int texOffsetPrev = ((segl->offset - IMFixMul( finetangent[(segl->CenterAngle+xtoviewangle[x])>>ANGLETOFINESHIFT], segl->distance)) >> FRACBITS) & (texWidth - 1);
    //int texLength;

    wallpartsCount = 0;
    wt->xLeft = x;
    wt->scaleLeft = *scaleData;
    wt->textureOffset = texOffsetPrev;
    do {
        int texOffset = ((segl->offset - IMFixMul( finetangent[(segl->CenterAngle+xtoviewangle[x + 1])>>ANGLETOFINESHIFT], segl->distance)) >> FRACBITS) & (texWidth - 1);
        if (texOffset < texOffsetPrev) {
            wt->xRight = x + 1; // to fill some gaps
            wt->scaleRight = *scaleData;
            //texLength = texOffsetPrev - wt->textureOffset + 1;
            //if (texLength < 1) texLength = 1;
            //wt->textureLength = texLength;
            wt->textureLength = texOffsetPrev - wt->textureOffset + 1;

            ++wt;
            wt->xLeft = x + 1; //because it's the next column compared to the current one
            wt->scaleLeft = *(scaleData + 1);
            wt->textureOffset = texOffset;

            ++wallpartsCount;
        }

        texOffsetPrev = texOffset;
        ++scaleData;
    } while(++x < segl->RightX);

    wt->xRight = x + 1; // to fill some gaps
    wt->scaleRight = *(scaleData - 1);  // because we ++scaleData before exiting
    //texLength = texOffsetPrev - wt->textureOffset + 1;
    //if (texLength < 1) texLength = 1;
    //wt->textureLength = texLength;
    wt->textureLength = texOffsetPrev - wt->textureOffset + 1;
    ++wallpartsCount;
}

static void PrepareWallparts(viswall_t *segl, Word texWidth, int *scaleData)
{
    int texLeft = (segl->offset - IMFixMul( finetangent[(segl->CenterAngle+xtoviewangle[segl->LeftX])>>ANGLETOFINESHIFT], segl->distance)) >> FRACBITS;
    int texRight = (segl->offset - IMFixMul( finetangent[(segl->CenterAngle+xtoviewangle[segl->RightX])>>ANGLETOFINESHIFT], segl->distance)) >> FRACBITS;

    doWireTex = false;
    if (texLeft > -3 && texRight < texWidth + 2) {
        int texOffset, texLength;
        wallpart_t *wt = wallparts;

        if (texLeft < 0) texLeft = 0;
        if (texRight > texWidth - 1) texRight = texWidth - 1;
        texOffset = texLeft;
        texLength = texRight - texLeft + 1;

        if (texLength < 1) texLength = 1;

        wt->textureOffset = texOffset;
        wt->textureLength = texLength;

        wt->scaleLeft = *scaleData;
        wt->scaleRight = *(scaleData + segl->RightX - segl->LeftX);

        wt->xLeft = segl->LeftX;
        wt->xRight = segl->RightX + 1;

        wallpartsCount = 1;
        ++fullWallPartCount;
        doWireTex = true;
    } else {
        PrepareBrokenWallparts(segl, texWidth, scaleData);
        brokenWallPartCount +=wallpartsCount;
    }
}

static void PrepareWallpartsLo(viswall_t *segl, Word texWidth, int *scaleData)
{
    int texLeft = (segl->offset - IMFixMul( finetangent[(segl->CenterAngle+xtoviewangle[segl->LeftX])>>ANGLETOFINESHIFT], segl->distance)) >> FRACBITS;
    int texRight = (segl->offset - IMFixMul( finetangent[(segl->CenterAngle+xtoviewangle[segl->RightX])>>ANGLETOFINESHIFT], segl->distance)) >> FRACBITS;

    wallpart_t *wt = wallparts;

    doWireTex = false;
    if (texLeft > -3 && texRight < texWidth + 2) {
        int texOffset, texLength;

        if (texLeft < 0) texLeft = 0;
        if (texRight > texWidth - 1) texRight = texWidth - 1;
        texOffset = texLeft;
        texLength = texRight - texLeft + 1;

        if (texLength < 1) texLength = 1;

        wt->textureOffset = texOffset;
        wt->textureLength = texLength;

        wt->scaleLeft = *scaleData;
        wt->scaleRight = *(scaleData + segl->RightX - segl->LeftX);

        wt->xLeft = segl->LeftX;
        wt->xRight = segl->RightX + 1;

        ++fullWallPartCount;
        doWireTex = true;
    } else {
        wt->textureOffset = 0;
        wt->textureLength = texWidth - 1;

        wt->scaleLeft = *scaleData;
        wt->scaleRight = *(scaleData + segl->RightX - segl->LeftX);

        wt->xLeft = segl->LeftX;
        wt->xRight = segl->RightX + 1;

        ++brokenWallPartCount;
    }
    wallpartsCount = 1;
}

static void PrepareWallpartsFlat(viswall_t *segl, int *scaleData)
{
    wallpart_t *wt = wallparts;
    const int length = segl->RightX - segl->LeftX;

    wt->scaleLeft = *scaleData;
    wt->scaleRight = *(scaleData + length);

    wt->xLeft = segl->LeftX;
    wt->xRight = segl->RightX + 1;

    wallpartsCount = 1;
}

static void DrawSegAnyLL(viswall_t *segl, int *scaleData, bool isTop)
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

    if (opt_wallQuality >= WALL_QUALITY_MED) {
        if (opt_wallQuality == WALL_QUALITY_HI) {
            PrepareWallparts(segl, tex->width, scaleData);
        } else {
            PrepareWallpartsLo(segl, tex->width, scaleData);
        }
        DrawWallSegmentTexturedLL(&drawtex, CenterY);
    } else {
        PrepareWallpartsFlat(segl, scaleData);
        DrawWallSegmentFlatLL(&drawtex, CenterY);
    }
}

void DrawSegUnshadedLL(viswall_t *segl, int *scaleData)
{
    Word ActionBits = segl->WallActions;
	if (!(ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))) return;

    ambientLight = segl->seglightlevel;
    if (opt_depthShading == DEPTH_SHADING_DARK) ambientLight = lightmins[ambientLight];


    if (ActionBits&AC_TOPTEXTURE)
        DrawSegAnyLL(segl, scaleData, true);

    if (ActionBits&AC_BOTTOMTEXTURE)
        DrawSegAnyLL(segl, scaleData, false);
}
