#include "Doom.h"
#include <IntMath.h>

#include "string.h"

#define WALL_PARTS_MAX (MAXWALLCMDS)
#define CCB_ARRAY_WALL_POLY_MAX (WALL_PARTS_MAX * 2)

// OLD POLY (trick to avoid Y tiling) fails on real hardware (black textures and FPS almost freezes)
//#define OLD_POLY

//#define DO_IMFIXMUL
//#define DO_IMFIXMUL_ON_EDGES		// needed for not having polygon bugs/overflows (but why is the above working?)


typedef struct {
    int xLeft, xRight;
    int scaleLeft, scaleRight;
    int textureOffset, textureLength;
    Word shadeLight;
} wallpart_t;

static wallpart_t wallParts[WALL_PARTS_MAX];
static int wallPartsCount;

static int texColumnOffset[MAXSCREENWIDTH];
static bool texColumnOffsetPrepared;
static int CCBflagsCurrentAlteredIndex = 0;

static drawtex_t drawtex;

static Word pixcLight;

static PolyCCB CCBQuadWallFlat;
static PolyCCB CCBQuadWallTextured[CCB_ARRAY_WALL_POLY_MAX];
static int CCBArrayWallPolyCurrent = 0;

static const int flatTexWidth = 8;  static const int flatTexWidthShr = 3;
static const int flatTexHeight = 1;  static const int flatTexHeightShr = 0;
static const int flatTexStride = 8;
static unsigned char *texBufferFlat;

static const int mode8bpp = 5;
static const int mode4bpp = 3;

static uint16 coloredPolyWallPals[16];
static Word *LightTablePtr = LightTable;

#define RECIPROCAL_MAX_NUM 1024
#define RECIPROCAL_FP 16
static int reciprocalLength[RECIPROCAL_MAX_NUM];

static int texLeft, texRight;

void initCCBQuadWallFlat()
{
    const int flatTexSize = flatTexStride * flatTexHeight;
    texBufferFlat = (unsigned char*)AllocAPointer(flatTexSize * sizeof(unsigned char));
    memset(texBufferFlat, 0, flatTexSize);

    CCBQuadWallFlat.ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|/*CCB_NOBLK|*/CCB_PPABS|CCB_LDPLUT|CCB_ACSC|CCB_ALSC|CCB_LAST;
    CCBQuadWallFlat.ccb_PRE0 = mode8bpp | ((flatTexHeight - 1) << 6);
    CCBQuadWallFlat.ccb_PRE1 = (((flatTexStride >> 2) - 2) << 16) | (flatTexWidth - 1) | 0x5000;

    CCBQuadWallFlat.ccb_HDX = 0;
    CCBQuadWallFlat.ccb_HDDX = 0;

    CCBQuadWallFlat.ccb_SourcePtr = (CelData*)texBufferFlat;
}

void initCCBQuadWallTextured()
{
	PolyCCB *CCBPtr;
	int i;

	CCBPtr = CCBQuadWallTextured;
	for (i=0; i<CCB_ARRAY_WALL_POLY_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (PolyCCB *)(sizeof(PolyCCB)-8);	// Create the next offset

        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|/*CCB_NOBLK|*/CCB_PPABS|CCB_ACSC|CCB_ALSC;
        //if (i==0) CCBPtr->ccb_Flags |= CCB_LDPLUT;  // First CEL column will set the palette and shading for the rest

        CCBPtr->ccb_HDX = 0;
        CCBPtr->ccb_HDDX = 0;

        ++CCBPtr;
	}

    for (i=0; i<RECIPROCAL_MAX_NUM; ++i) {
        reciprocalLength[i] = (1 << RECIPROCAL_FP) / i;
    }
}

static void DrawWallSegmentFlatPoly(drawtex_t *tex, void *texPal)
{
	int topLeft, topRight;
	int bottomLeft, bottomRight;
	int lengthLeft, lengthRight, lengthDiff;

    const int run = (tex->topheight - tex->bottomheight) >> HEIGHTBITS;

    wallpart_t *wp = wallParts;

    const int xLeft = wp->xLeft;
    const int xRight = wp->xRight;
    const int xLength = xRight - xLeft;

    int scaleLeft;
    int scaleRight;

    if (run <= 0) return;
    if (xLength < 1) return;


	scaleLeft = wp->scaleLeft;
    scaleRight = wp->scaleRight;

	topLeft = CenterY - ((scaleLeft * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
	topRight = CenterY - ((scaleRight * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
	bottomLeft = topLeft + ((run * scaleLeft) >> SCALEBITS) + 1;
	bottomRight = topRight + ((run * scaleRight) >> SCALEBITS) + 1;


    lengthLeft = bottomLeft - topLeft + 1;
    lengthRight = bottomRight - topRight + 1;
    lengthDiff = lengthRight - lengthLeft;


    CCBQuadWallFlat.ccb_XPos = xLeft << 16;
    CCBQuadWallFlat.ccb_YPos = bottomLeft << 16;
    CCBQuadWallFlat.ccb_VDX = (xLength << (16 - flatTexHeightShr));
    CCBQuadWallFlat.ccb_VDY = (bottomRight - bottomLeft) << (16 - flatTexHeightShr);

    CCBQuadWallFlat.ccb_HDX = 0;
    CCBQuadWallFlat.ccb_HDY = -lengthLeft << (20 - flatTexWidthShr);

    CCBQuadWallFlat.ccb_HDDX = 0;
    CCBQuadWallFlat.ccb_HDDY = -lengthDiff << (20 - flatTexWidthShr - flatTexHeightShr);


    CCBQuadWallFlat.ccb_PLUTPtr = texPal;
    CCBQuadWallFlat.ccb_PIXC = pixcLight;

    DrawCels(VideoItem,(CCB*)&CCBQuadWallFlat);
}

void drawCCBarrayPoly(Word xEnd)
{
    PolyCCB *polyCCBstart, *polyCCBend;

	polyCCBstart = &CCBQuadWallTextured[0];    // First poly CEL of the wall segment
	polyCCBend = &CCBQuadWallTextured[xEnd];   // Last poly CEL of the wall segment

	polyCCBend->ccb_Flags |= CCB_LAST;         // Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,(CCB*)polyCCBstart);    // Draw all the cels of a single wall in one shot
    polyCCBend->ccb_Flags ^= CCB_LAST;         // remember to flip off that CCB_LAST flag, since we don't reinit the flags for all polys every time
}

void flushCCBarrayPolyWall()
{
	if (CCBArrayWallPolyCurrent != 0) {
		int i;
		drawCCBarrayPoly(CCBArrayWallPolyCurrent - 1);
		//printDbgMinMax(CCBArrayWallPolyCurrent);
		CCBArrayWallPolyCurrent = 0;

		for (i=0; i<CCBflagsCurrentAlteredIndex; ++i) {
			*CCBflagsAlteredIndexPtr[i] &= ~CCB_LDPLUT;
		}
		CCBflagsCurrentAlteredIndex = 0;
	}
}

static void DrawWallSegmentTexturedQuadSubdivided(drawtex_t *tex, int run, Word pre0part, Word pre1part, Word frac)
{
    PolyCCB *CCBPtr;

    Byte *texBitmap = &tex->data[32];

    wallpart_t *wp = wallParts;

    int polysCount = wallPartsCount;

    const int recHeight = reciprocalLength[run];

    if (polysCount < 1) return;
	//if (polysCount > 1) texBitmap = 0;

	if (CCBArrayWallPolyCurrent + polysCount >= CCB_ARRAY_WALL_POLY_MAX) {
		flushCCBarrayPolyWall();
	}

    CCBPtr = &CCBQuadWallTextured[CCBArrayWallPolyCurrent];
	CCBArrayWallPolyCurrent += polysCount;
    do {
        int topLeft, topRight;
        int lengthLeft, lengthRight, lengthDiff;

        Word colnum;

        const int texLength = wp->textureLength;
        const int recWidth = reciprocalLength[texLength];

        const int xLeft = wp->xLeft;
        const int xRight = wp->xRight;
        const int xLength = xRight - xLeft;

        const int scaleLeft = wp->scaleLeft;
        const int scaleRight = wp->scaleRight;

        const int textureOffset = wp->textureOffset;

        ++wp;

        if (xLength < 1) continue;

        colnum = textureOffset;	// Get the starting column offset
        colnum = (colnum*tex->height)+frac;	// Index to the shape

        colnum >>= 1;           // Pixel to byte offset
        colnum &= ~3;			// Long word align the source

        if (optGraphics->depthShading >= DEPTH_SHADING_DITHERED) {
            int textureLight = ((scaleLeft*lightcoef)>>16) - lightsub;
            if (textureLight < lightmin) textureLight = lightmin;
            if (textureLight > lightmax) textureLight = lightmax;
            pixcLight = LightTablePtr[textureLight>>LIGHTSCALESHIFT];
        }

        topLeft = CenterY - ((scaleLeft * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;
        topRight = CenterY - ((scaleRight * tex->topheight) >> (HEIGHTBITS+SCALEBITS)) - 1;

        lengthLeft = ((run * scaleLeft) >> SCALEBITS) + 2;
        lengthRight = ((run * scaleRight) >> SCALEBITS) + 2;
        lengthDiff = lengthRight - lengthLeft;

        CCBPtr->ccb_XPos = xLeft << 16;
        CCBPtr->ccb_YPos = topLeft << 16;
        CCBPtr->ccb_VDX = ((xLength * recWidth) << (16 - RECIPROCAL_FP))  + ((recWidth >> 2) << (16 - RECIPROCAL_FP)); // >>2 or a quarter of a pixel seems to eliminate edge overdrawing without leaving gaps, don't know why (checked by enabling blending over every surface)
        CCBPtr->ccb_VDY = ((topRight - topLeft) * recWidth) << (16 - RECIPROCAL_FP);

        CCBPtr->ccb_HDY = (lengthLeft * recHeight) << (20 - RECIPROCAL_FP);

        CCBPtr->ccb_HDDY = ((lengthDiff * recWidth) << (20 - RECIPROCAL_FP)) / run;


        CCBPtr->ccb_SourcePtr = (CelData*)&texBitmap[colnum];
        CCBPtr->ccb_PIXC = pixcLight;// | 0x0080; // alpha test for overdrawing


        CCBPtr->ccb_PRE0 = pre0part | ((texLength - 1) << 6);
        CCBPtr->ccb_PRE1 = pre1part | (run - 1);

        ++CCBPtr;
    } while(--polysCount != 0);
}


static void DrawWallSegmentTexturedQuad(drawtex_t *tex, void *texPal, viswall_t *segl)
{
    Word frac;
    Word colnum7;
    Word pre0part, pre1part;

    int texHeight = tex->height;
    int run = (tex->topheight - tex->bottomheight) >> HEIGHTBITS;

    Word texStride = texHeight >> 1;
    if (texStride < 8) texStride = 8;

    if (run <= 0 || run >= RECIPROCAL_MAX_NUM) return;

    if (optGraphics->depthShading >= DEPTH_SHADING_DITHERED) {
        const int lightIndex = segl->seglightlevelContrast;

        lightmin = lightmins[lightIndex];
        lightmax = lightIndex;
        lightsub = lightsubs[lightIndex];
        lightcoef = lightcoefs[lightIndex];
    }

    frac = tex->texturemid - (tex->topheight<<FIXEDTOHEIGHT);	// Get the anchor point
    frac >>= FRACBITS;
    while (frac&0x8000) {
        frac += texHeight;		// Make sure it's on the shape
    }
    frac&=0x7f;		// Zap unneeded bits
    colnum7 = frac & 7;     // Get the pixel skip


    if (run + colnum7 > texHeight) {
        colnum7 = 0;   // temporary hack for some tall walls that are buggy (and can't figure logic atm). Don't do it for shorter walls, it will make doors jerky.
    } else {
        run += colnum7;
    }

    pre0part = (colnum7 << 24) | mode4bpp;
    pre1part = (((texStride >> 2) - 2) << 24) | 0x5000; // 5000 for the LSB of blue to match exact colors as in original Doom column renderer

	CCBQuadWallTextured[CCBArrayWallPolyCurrent].ccb_PLUTPtr = texPal;    // plut pointer only for first element
	CCBQuadWallTextured[CCBArrayWallPolyCurrent].ccb_Flags |= CCB_LDPLUT;

	CCBflagsAlteredIndexPtr[CCBflagsCurrentAlteredIndex++] = &CCBQuadWallTextured[CCBArrayWallPolyCurrent].ccb_Flags;

	#ifdef OLD_POLY
	// OLD Poly, no Y tiling (fails on real hardware)
		DrawWallSegmentTexturedQuadSubdivided(tex, run, pre0part, pre1part, frac);
	#else
	// New Poly, Y tiling
	while(run > texHeight) {
		DrawWallSegmentTexturedQuadSubdivided(tex, texHeight, pre0part, pre1part, frac);
		tex->topheight -= (texHeight << HEIGHTBITS);
		run-= texHeight;
	};
	if (run > 0)
		DrawWallSegmentTexturedQuadSubdivided(tex, run, pre0part, pre1part, frac);
	#endif
}


/**********************************

	Draw a single wall texture.
	Also save states for pending ceiling, floor and future clipping

**********************************/

static void calcColumnOffsets(viswall_t *segl)
{
	const int xLeft = segl->LeftX + 4;
    const int xRight = segl->RightX - 1;

	const Fixed offset = segl->offset;
	const angle_t centerAngle = segl->CenterAngle;

	#ifdef DO_IMFIXMUL
		const Word distance = segl->distance;
	#else
		const Word distance = segl->distance >> VISWALL_DISTANCE_PRESHIFT;
	#endif

	int *tc = texColumnOffset;
	int prevTexU = texLeft;
	int x;

    *tc++ = prevTexU;
	for (x=xLeft; x<=xRight; x+=4) {
		#ifdef DO_IMFIXMUL
			const int texU = (offset - IMFixMul( finetangent[(centerAngle+xtoviewangle[x])>>ANGLETOFINESHIFT], distance)) >> FRACBITS;
		#else
			const int texU = (offset-((finetangent[(centerAngle+xtoviewangle[x])>>ANGLETOFINESHIFT] * distance)>>(FRACBITS-VISWALL_DISTANCE_PRESHIFT)))>>FRACBITS;
		#endif
		const int texUhalf = (prevTexU + texU) >> 1;

		*tc++ = (prevTexU + texUhalf) >> 1;
		*tc++ = texUhalf;
		*tc++ = (texUhalf + texU) >> 1;
		*tc++ = texU;

		prevTexU = texU;
    }
	for (x-=3; x<=xRight; ++x) {
		*tc++ = (offset-((finetangent[(centerAngle+xtoviewangle[x])>>ANGLETOFINESHIFT] * distance)>>(FRACBITS-VISWALL_DISTANCE_PRESHIFT)))>>FRACBITS;
	}
	*tc = texRight;
}

static int offPoints[MAXWALLCMDS];

static void PrepareBrokenWallParts(viswall_t *segl, Word texWidth, ColumnStore *columnStoreData)
{
	const int xLeft = segl->LeftX;
    const int xRight = segl->RightX;
    int x,i;

    wallpart_t *wp = wallParts;

    int *texColOffset = texColumnOffset;
	int texOffset;
    int texOffsetPrev = *texColOffset & (texWidth - 1);
	int numMidPoints = 0;

	offPoints[numMidPoints++] = xLeft;

	/*
	// OLD SINGLE LOOP VERSION
	*texColOffset++;
	for (x = xLeft+1; x<xRight; ++x) {
		texOffset = *texColOffset++ & (texWidth - 1);
        if (texOffset < texOffsetPrev) {
			offPoints[numMidPoints++] = x;
		}
		texOffsetPrev = texOffset;
	}*/

	// ONE IN FOUR BIFURCATE VERSION
	texColOffset = &texColumnOffset[-xLeft];
	for (x = xLeft+4; x<xRight; x+=4) {
		texOffset = texColOffset[x] & (texWidth - 1);
        if (texOffset < texOffsetPrev) {
			for (i=-3; i<0; ++i) {
				const int subTexOffset = texColOffset[x+i] & (texWidth - 1);
				if (subTexOffset < texOffsetPrev) {
					x+=i;
					texOffset = subTexOffset;
					break;
				}
			}
			offPoints[numMidPoints++] = x;
		}
		texOffsetPrev = texOffset;
	}
	for (x-=3; x<xRight; ++x) {
		texOffset = texColOffset[x] & (texWidth - 1);
        if (texOffset < texOffsetPrev) {
			offPoints[numMidPoints++] = x;
		}
		texOffsetPrev = texOffset;
	}

	offPoints[numMidPoints] = xRight;

	for (i=0; i<numMidPoints; ++i) {
		const int x0 = offPoints[i];
		const int x1 = offPoints[i+1];
		const int index0 = x0-xLeft;
		const int index1 = x1-xLeft-1;
		wp->xLeft = x0;
		wp->xRight = x1;
		wp->scaleLeft = columnStoreData[index0].scale;
		wp->scaleRight = columnStoreData[index1].scale;

		if (i==0 || i==numMidPoints-1) {
			const int texColOffset0 = texColumnOffset[index0] & (texWidth - 1);
			const int texColOffset1 = texColumnOffset[index1] & (texWidth - 1);
			int length = texColOffset1 - texColOffset0 + 1;
			if (length > texWidth) length = texWidth;
			//if (length < 1) length = 1;

			wp->textureOffset = texColOffset0;
			wp->textureLength = length;
			if (i==numMidPoints-1) wp->xRight+=1;
		} else {
			wp->textureOffset = 0;
			wp->textureLength = texWidth;
		}
		wp++;
	}
	wallPartsCount = numMidPoints;
}

static void PrepareWallParts(viswall_t *segl, Word texWidth, ColumnStore *columnStoreData)
{
	int texL, texR;
	int texMinOff;

	const Word leftX = segl->LeftX;
	const Word rightX = segl->RightX;

	const Fixed offset = segl->offset;
	const angle_t centerAngle = segl->CenterAngle;

	//const int realOffset = segl->SegPtr->offset >> 16;
	//const int length = segl->SegPtr->length;

	#ifdef DO_IMFIXMUL_ON_EDGES
		const Word distance = segl->distance;
		texLeft = (offset - IMFixMul( finetangent[(centerAngle+xtoviewangle[leftX])>>ANGLETOFINESHIFT], distance)) >> FRACBITS;
		texRight = (offset - IMFixMul( finetangent[(centerAngle+xtoviewangle[rightX])>>ANGLETOFINESHIFT], distance)) >> FRACBITS;
	#else
		const Word distance = segl->distance >> VISWALL_DISTANCE_PRESHIFT;
		texLeft = (offset-((finetangent[(centerAngle+xtoviewangle[leftX])>>ANGLETOFINESHIFT] * distance)>>(FRACBITS-VISWALL_DISTANCE_PRESHIFT)))>>FRACBITS;
		texRight = (offset-((finetangent[(centerAngle+xtoviewangle[rightX])>>ANGLETOFINESHIFT] * distance)>>(FRACBITS-VISWALL_DISTANCE_PRESHIFT)))>>FRACBITS;
	#endif

	/*if (texLeft < realOffset) {	// a bit of stitching at over 2
		texLeft = realOffset;
	}
	if (texRight > realOffset + length - 1) {	// a bit of stitching at under 2
		texRight = realOffset + length - 1;
	}*/

	texL = texLeft & (texWidth - 1);
	texMinOff = texLeft - texL;
	texR = texRight - texMinOff;

	if (texR <= texWidth) {	// little precision might get some textures just right over their max U coord, even if they fit exactly in level data
		int texLength;
		wallpart_t *wp = wallParts;

		texLength = texR - texL + 1;
		if (texLength < 1) texLength = 1;

		wp->textureOffset = texL;
		wp->textureLength = texLength;

		wp->scaleLeft = columnStoreData->scale;
		wp->scaleRight = (columnStoreData + rightX - leftX)->scale;

		wp->xLeft = leftX;
		wp->xRight = rightX + 1;

		wallPartsCount = 1;
	} else {
		if (!texColumnOffsetPrepared) {
			calcColumnOffsets(segl);
			texColumnOffsetPrepared = true;
		}
		PrepareBrokenWallParts(segl, texWidth, columnStoreData);
	}
}

static void PrepareWallPartsFlat(viswall_t *segl, ColumnStore *columnStoreData)
{
    wallpart_t *wp = wallParts;
	const Word leftX = segl->LeftX;
	const Word rightX = segl->RightX;
    const int length = rightX - leftX;

    wp->scaleLeft = columnStoreData->scale;
    wp->scaleRight = (columnStoreData + length)->scale;

    wp->xLeft = leftX;
    wp->xRight = rightX + 1;
}

static void DrawSegAnyPoly(viswall_t *segl, ColumnStore *columnStoreData, bool isTop, bool shouldPrepareWallParts)
{
    texture_t *tex;
    void *texPal;

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

	if (segl->special & SEC_SPEC_FOG) {
		LightTablePtr = LightTableFog;
	} else {
		LightTablePtr = LightTable;
	}

	if (optGraphics->wallQuality == WALL_QUALITY_LO || optGraphics->depthShading < DEPTH_SHADING_DITHERED) {
		Word ambientLight = segl->seglightlevelContrast;
		if (optGraphics->depthShading == DEPTH_SHADING_DARK) ambientLight = lightmins[ambientLight];
		pixcLight = LightTablePtr[ambientLight>>LIGHTSCALESHIFT];
	}

    if (optGraphics->wallQuality > WALL_QUALITY_LO) {
		if (segl->color==0) {
			texPal = drawtex.data;
		} else {
			texPal = coloredPolyWallPals;
			initColoredPals((uint16*)drawtex.data, texPal, 16, segl->color);
		}
        if (shouldPrepareWallParts) {
			PrepareWallParts(segl, tex->width, columnStoreData);
		}
        if (wallPartsCount > 0 && wallPartsCount < WALL_PARTS_MAX) {
			DrawWallSegmentTexturedQuad(&drawtex, texPal, segl);
		}
    } else {
		if (segl->color==0) {
			texPal = &tex->color;
		} else {
			texPal = coloredPolyWallPals;
			initColoredPals((uint16*)&tex->color, texPal, 1, segl->color);
		}

        PrepareWallPartsFlat(segl, columnStoreData);
        DrawWallSegmentFlatPoly(&drawtex, texPal);
    }

	// Color walls changed in a common palette will need frequent flush unfortunatelly
	if (texPal == coloredPolyWallPals) {
		flushCCBarrayPolyWall();
	}
}

void DrawSegPoly(viswall_t *segl, ColumnStore *columnStoreData)
{
    const Word ActionBits = segl->WallActions;
    const bool topTexOn = (bool)(ActionBits & AC_TOPTEXTURE);
    const bool bottomTexOn = (bool)(ActionBits & AC_BOTTOMTEXTURE);

    const bool shouldPrepareAgain = !(topTexOn && bottomTexOn && (segl->t_texture->width == segl->b_texture->width));

	if (!(topTexOn || bottomTexOn)) return;

    texColumnOffsetPrepared = false;

    if (topTexOn) {
        DrawSegAnyPoly(segl, columnStoreData, true, true);
	}

    if (bottomTexOn) {
        DrawSegAnyPoly(segl, columnStoreData, false, shouldPrepareAgain);
	}
}


// ============ Wireframe renderer ============


static void DrawWallSegmentWireframe(drawtex_t *tex)
{
	int topLeft, topRight;
	int bottomLeft, bottomRight;

    const int run = (tex->topheight - tex->bottomheight) >> HEIGHTBITS;

    const int screenCenterX = ScreenWidth >> 1;
    const int screenCenterY = ScreenHeight >> 1;

    const int xLeft = wallParts->xLeft - screenCenterX;
    const int xRight = wallParts->xRight - screenCenterX;
    const int xLength = xRight - xLeft;

    const int scaleLeft = wallParts->scaleLeft;
    const int scaleRight = wallParts->scaleRight;

    const Word color = *((Word*)tex->data);

    if (run <= 0) return;
    if (xLength < 1) return;

	topLeft = ScreenHeight - (CenterY - ((scaleLeft * tex->topheight) >> (HEIGHTBITS+SCALEBITS))) - screenCenterY;
	topRight = ScreenHeight - (CenterY - ((scaleRight * tex->topheight) >> (HEIGHTBITS+SCALEBITS))) - screenCenterY;
	bottomLeft = topLeft - ((run * scaleLeft) >> SCALEBITS);
	bottomRight = topRight - ((run * scaleRight) >> SCALEBITS);

    DrawThickLine(xLeft, topLeft, xRight, topRight, color);
    DrawThickLine(xRight, topRight, xRight, bottomRight, color);
    DrawThickLine(xRight, bottomRight, xLeft, bottomLeft, color);
    DrawThickLine(xLeft, bottomLeft, xLeft, topLeft, color);
}

static void DrawSegWireframeAny(viswall_t *segl, ColumnStore *columnStoreData, bool isTop)
{
    texture_t *tex;
    if (isTop) {
        tex = segl->t_texture;

        drawtex.topheight = segl->t_topheight;
        drawtex.bottomheight = segl->t_bottomheight;
    } else {
        tex = segl->b_texture;

        drawtex.topheight = segl->b_topheight;
        drawtex.bottomheight = segl->b_bottomheight;
    }
    drawtex.data = (Byte *)*tex->data;

    PrepareWallPartsFlat(segl, columnStoreData);
    DrawWallSegmentWireframe(&drawtex);
}

void DrawSegWireframe(viswall_t *segl, ColumnStore *columnStoreData)
{
    const Word ActionBits = segl->WallActions;
    const bool topTexOn = (bool)(ActionBits & AC_TOPTEXTURE);
    const bool bottomTexOn = (bool)(ActionBits & AC_BOTTOMTEXTURE);

	if (!(topTexOn || bottomTexOn)) return;

    if (topTexOn)
        DrawSegWireframeAny(segl, columnStoreData, true);

    if (bottomTexOn)
        DrawSegWireframeAny(segl, columnStoreData, false);
}
