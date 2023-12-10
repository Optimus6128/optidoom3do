#include "Doom.h"
#include <IntMath.h>

#define OPENMARK ((MAXSCREENHEIGHT-1)<<8)

static BitmapCCB CCBArraySky[MAXSCREENWIDTH];       // Array of CCB struct for the sky columns

typedef struct {
	int scale;
	int ceilingclipy;
	int floorclipy;
} segloop_t;


static int clipboundtop[MAXSCREENWIDTH];		// Bounds top y for vertical clipping
static int clipboundbottom[MAXSCREENWIDTH];	// Bounds bottom y for vertical clipping

segloop_t segloops[MAXSCREENWIDTH];

bool skyOnView;                         // marker to know if at one frame sky was visible or not


void initCCBarraySky(void)
{
	BitmapCCB *CCBPtr;
	int i;

	const int skyScale = getSkyScale();

	CCBPtr = CCBArraySky;
	for (i=0; i<MAXSCREENWIDTH; ++i) {
		CCBPtr->ccb_NextPtr = (BitmapCCB *)(sizeof(BitmapCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
                            CCB_ACE|CCB_BGND|/*CCB_NOBLK|*/CCB_PPABS;	// ccb_flags

        if (i==0) CCBPtr->ccb_Flags |= CCB_LDPLUT;  // First CEL column will set the palette for the rest

        CCBPtr->ccb_PRE0 = 0x03;
        CCBPtr->ccb_PRE1 = 0x3E005000|(128-1);  // Project the pixels
        CCBPtr->ccb_YPos = 0<<16;
        CCBPtr->ccb_HDX = 0<<20;		        // Convert 6 bit frac to CCB scale
        CCBPtr->ccb_HDY = skyScale;             // Video stretch factor
        CCBPtr->ccb_VDX = 1<<16;
        CCBPtr->ccb_VDY = 0<<16;
        CCBPtr->ccb_PIXC = 0x1F00;              // PIXC control

		++CCBPtr;
	}
}


/**********************************

	Given a span of pixels, see if it is already defined
	in a record somewhere. If it is, then merge it otherwise
	make a new plane definition.

**********************************/

int visplanesCountMax = 0;
static bool isFloor;

static visplane_t *FindPlane(visplane_t *check, viswall_t *segl, int start, Word color)
{
	const Fixed height = segl->floorheight;
	void **PicHandle = segl->FloorPic;
	const int stop = segl->RightX;
	const Word Light = segl->seglightlevel;
	const Word special = segl->special & SEC_SPEC_RENDER_BITS;
	
	if (visplanesCount > maxVisplanes-2) return 0;

	++check;		/* Automatically skip to the next plane */
	if (check<lastvisplane) {
		do {
			if (height == check->height &&		/* Same plane as before? */
				PicHandle == check->PicHandle &&
				Light == check->PlaneLight && 
				color == check->color && 
				special == check->special && 
				check->open[start] == OPENMARK) {	/* Not defined yet? */
				if (start < check->minx) {	/* In range of the plane? */
					check->minx = start;	/* Mark the new edge */
				}
				if (stop > check->maxx) {
					check->maxx = stop;		/* Mark the new edge */
				}
				return check;			/* Use the same one as before */
			}
		} while (++check<lastvisplane);
	}

/* make a new plane */

	check = lastvisplane;
	++lastvisplane;
	++visplanesCount;
	check->height = height;		/* Init all the vars in the visplane */
	check->PicHandle = PicHandle;
	check->color = color;
	check->special = special;
	check->isFloor = isFloor;
	check->minx = start;
	check->maxx = stop;
	check->PlaneLight = Light;		/* Set the light level */

	if (visplanesCount > visplanesCountMax) visplanesCountMax = visplanesCount;

/* Quickly fill in the visplane table */
    {
        Word i, j;
        Word *set;

        i = OPENMARK;
        set = check->open;	/* A brute force method to fill in the visplane record FAST! */
        j = ScreenWidth/4;
        do {
            set[0] = i;
            set[1] = i;
            set[2] = i;
            set[3] = i;
            set+=4;
        } while (--j);

        check->miny = MAXSCREENHEIGHT;
        check->maxy = -1;
    }
	return check;
}


/**********************************

	Do a fake wall rendering so I can get all the visplane records.
	This is a fake-o routine so I can later draw the wall segments from back to front.

**********************************/

static void SegLoopFloor(viswall_t *segl, Word screenCenterY)
{
	Word x;

	visplane_t *FloorPlane;
	Word color;
	segloop_t *segdata = segloops;
	
	const Word rightX = segl->RightX;
	const int floorHeight = segl->floorheight;

	if (optGraphics->planeQuality == PLANE_QUALITY_LO) {
		const Word floorColor = segl->floorAndCeilingColor >> 16;
		color = (floorColor << 16) | floorColor | (1 << 15); // high bit also for dithered flat two color checkerboard palette
	} else {
		color = segl->color;
	}

	FloorPlane = visplanes;		// Reset the visplane pointers
	isFloor = true;

	x = segl->LeftX;
	do {
		int top, bottom;
		const int scale = segdata->scale;
		const int ceilingclipy = segdata->ceilingclipy;
		const int floorclipy = segdata->floorclipy;

        top = screenCenterY - ((scale * floorHeight)>>(HEIGHTBITS+SCALEBITS));	// Y coord of top of floor
        if (top <= ceilingclipy) {
            top = ceilingclipy+1;		// Clip the top of floor to the bottom of the visible area
        }
        bottom = floorclipy-1;		// Draw to the bottom of the screen
        if (top <= bottom) {		// Valid span?
            if (FloorPlane->open[x] != OPENMARK) {	// Not already covered?
                FloorPlane = FindPlane(FloorPlane, segl, x, color);
                if (FloorPlane == 0) return;
            }
            if (top) {
                --top;
            }
            FloorPlane->open[x] = (top<<8)+bottom;	// Set the new vertical span
            if (FloorPlane->miny > top) FloorPlane->miny = top;
            if (FloorPlane->maxy < bottom) FloorPlane->maxy = bottom;
        }
        segdata++;
	} while (++x<=rightX);
}

static void SegLoopCeiling(viswall_t *segl, Word screenCenterY)
{
	Word x;

	visplane_t *CeilingPlane;
	Word color;
	segloop_t *segdata = segloops;
	
	const Word rightX = segl->RightX;
	const int ceilingHeight = segl->ceilingheight;

	if (optGraphics->planeQuality == PLANE_QUALITY_LO) {
		const Word ceilingColor = segl->floorAndCeilingColor & 0x0000FFFF;
		color = (ceilingColor << 16) | ceilingColor | (1 << 15); // high bit also for dithered flat two color checkerboard palette
	} else {
		color = segl->color;
	}

	CeilingPlane = visplanes;		// Reset the visplane pointers
	isFloor = false;

	// Ugly hack for the case FindPlane expects segl, but reads always floor (to not pass too many arguments as before and also not duplicate)
	segl->floorheight = ceilingHeight;
	segl->FloorPic = segl->CeilingPic;

	x = segl->LeftX;
	do {
		int top, bottom;
		const int scale = segdata->scale;
		const int ceilingclipy = segdata->ceilingclipy;
		const int floorclipy = segdata->floorclipy;

        top = ceilingclipy+1;		// Start from the ceiling
        bottom = (screenCenterY-1) - ((scale * ceilingHeight)>>(HEIGHTBITS+SCALEBITS));	// Bottom of the height
        if (bottom >= floorclipy) {		// Clip the bottom?
            bottom = floorclipy-1;
        }
        if (top <= bottom) {
            if (CeilingPlane->open[x] != OPENMARK) {		// Already in use?
                CeilingPlane = FindPlane(CeilingPlane, segl, x, color);
                if (CeilingPlane == 0) return;
            }
            if (top) {
                --top;
            }
            CeilingPlane->open[x] = (top<<8)+bottom;		// Set the vertical span
            if (CeilingPlane->miny > top) CeilingPlane->miny = top;
            if (CeilingPlane->maxy < bottom) CeilingPlane->maxy = bottom;
        }
        segdata++;
	} while (++x<=rightX);
}

static void SegLoopSpriteClipsBottom(viswall_t *segl, Word screenCenterY)
{
    Word ActionBits = segl->WallActions;
	segloop_t *segdata = segloops;

	const int floorNewHeight = segl->floornewheight;

	Word x = segl->LeftX;
	Byte *bottomSil = &segl->BottomSil[x];

	const Word rightX = segl->RightX;
	do {
        int low = screenCenterY - ((segdata->scale * floorNewHeight)>>(HEIGHTBITS+SCALEBITS));
        const int floorclipy = segdata->floorclipy;

        if (low > floorclipy) {
            low = floorclipy;
        } else if (low < 0) {
            low = 0;
        }
        if (ActionBits & AC_BOTTOMSIL) {
            *bottomSil++ = low;
        }
        if (ActionBits & AC_NEWFLOOR) {
            clipboundbottom[x] = low;
        }
		segdata++;
	} while (++x<=rightX);
}

static void SegLoopSpriteClipsTop(viswall_t *segl, Word screenCenterY)
{
    Word ActionBits = segl->WallActions;
	segloop_t *segdata = segloops;

	const int ceilingNewHeight = segl->ceilingnewheight;

	Word x = segl->LeftX;
	Byte *topSil = &segl->TopSil[x];

	const Word rightX = segl->RightX;
	do {
        int high = (screenCenterY-1) - ((segdata->scale * ceilingNewHeight)>>(HEIGHTBITS+SCALEBITS));
        const int ceilingclipy = segdata->ceilingclipy;

        if (high < ceilingclipy) {
            high = ceilingclipy;
        } else if (high > (int)ScreenHeight-1) {
            high = ScreenHeight-1;
        }
        if (ActionBits & AC_TOPSIL) {
            *topSil++ = high+1;
        }
        if (ActionBits & AC_NEWCEILING) {
            clipboundtop[x] = high;
        }
		segdata++;
	} while (++x<=rightX);
}

static void SegLoopSky(viswall_t *segl, Word screenCenterY)
{
	int scale;
	int ceilingclipy, floorclipy;
	int bottom;

	BitmapCCB *CCBPtr = &CCBArraySky[0];
    Byte *Source = (Byte *)(*SkyTexture->data);

	segloop_t *segdata = segloops;

	const int ceilingHeight = segl->ceilingheight;

    Word x = segl->LeftX;
	const Word rightX = segl->RightX;
    do {
		scale = segdata->scale;
		ceilingclipy = segdata->ceilingclipy;
		floorclipy = segdata->floorclipy;

        bottom = screenCenterY - ((scale * ceilingHeight)>>(HEIGHTBITS+SCALEBITS));
        if (bottom > floorclipy) {
            bottom = floorclipy;
        }
        if ((ceilingclipy+1) < bottom) {		// Valid?
            CCBPtr->ccb_XPos = x<<16;                               // Set the x and y coord for start
            CCBPtr->ccb_SourcePtr = (CelData *)&Source[((((xtoviewangle[x]+viewangle)>>ANGLETOSKYSHIFT)&0xFF)<<6) + 32];	// Get the source ptr
            ++CCBPtr;
        }
        segdata++;
	} while (++x<=rightX);


	if (CCBPtr != &CCBArraySky[0]) {
        CCBArraySky[0].ccb_PLUTPtr = Source;    // plut pointer only for first element
        drawCCBarray(--CCBPtr, CCBArraySky);
	}
}

static void prepColumnStoreDataPoly(viswall_t *segl)
{
	Word x = segl->LeftX;
	const Word rightX = segl->RightX;

	int _scalefrac = segl->LeftScale;		// Init the scale fraction
	const int _scalestep = segl->ScaleStep;

    segloop_t *segdata = segloops;
    ColumnStore *columnStoreData = columnStoreArrayData;

	do {
        int scale = _scalefrac>>FIXEDTOSCALE;	// Current scaling factor
		if (scale >= 0x2000) {		// Too large?
			scale = 0x1fff;			// Fix the scale to maximum
		}

		columnStoreData->scale = scale;
		columnStoreData++;
		segdata->scale = scale;
		segdata->ceilingclipy = clipboundtop[x];	// Get the top y clip
		segdata->floorclipy = clipboundbottom[x];	// Get the bottom y clip
        segdata++;

        _scalefrac += _scalestep;		// Step to the next scale
	} while (++x<=rightX);

	columnStoreArrayData = columnStoreData;
}

static void prepColumnStoreDataUnlit(viswall_t *segl, bool forceDark)
{
	Word x = segl->LeftX;
	const Word rightX = segl->RightX;

	int _scalefrac = segl->LeftScale;		// Init the scale fraction
	const int _scalestep = segl->ScaleStep;

    segloop_t *segdata = segloops;
    ColumnStore *columnStoreData = columnStoreArrayData;

	int wallColumnLight = segl->seglightlevelContrast;
	if (forceDark || optGraphics->depthShading == DEPTH_SHADING_DARK) wallColumnLight = lightmins[wallColumnLight];

	do {
        int scale = _scalefrac>>FIXEDTOSCALE;	// Current scaling factor
		if (scale >= 0x2000) {		// Too large?
			scale = 0x1fff;			// Fix the scale to maximum
		}

		columnStoreData->scale = scale;
		columnStoreData->light = wallColumnLight;
		columnStoreData++;
		segdata->scale = scale;
		segdata->ceilingclipy = clipboundtop[x];	// Get the top y clip
		segdata->floorclipy = clipboundbottom[x];	// Get the bottom y clip
        segdata++;

        _scalefrac += _scalestep;		// Step to the next scale
	} while (++x<=rightX);
	columnStoreArrayData = columnStoreData;
}

static void prepColumnStoreDataLight(viswall_t *segl)
{
	Word x;
	const Word leftX = segl->LeftX + 4;
	const Word rightX = segl->RightX;

    ColumnStore *columnStoreData = columnStoreArrayData;

	const Word lightIndex = segl->seglightlevelContrast;
    const Fixed lightminF = lightmins[lightIndex];
    const Fixed lightmaxF = lightIndex;
	const Fixed lightsub = lightsubs[lightIndex];
	Fixed lightcoefF = ((segl->LeftScale >> 4) * (lightcoefs[lightIndex] >> 4)) >> (2 * FIXEDTOSCALE - 8);
	Fixed lightcoefFstep = 4 * (((segl->ScaleStep >> 4) * (lightcoefs[lightIndex] >> 4)) >> (2 * FIXEDTOSCALE - 8));

	int prevWallColumnLight = (lightcoefF >> (16 - FIXEDTOSCALE)) - lightsub;
	if (prevWallColumnLight < lightminF) prevWallColumnLight = lightminF;
	if (prevWallColumnLight > lightmaxF) prevWallColumnLight = lightmaxF;
	lightcoefF += lightcoefFstep;
	columnStoreData->light = prevWallColumnLight;
	columnStoreData++;

	for (x=leftX; x<=rightX; x+=4) {
		int wallColumnLightHalf;
		int wallColumnLight = (lightcoefF >> (16 - FIXEDTOSCALE)) - lightsub;
        if (wallColumnLight < lightminF) wallColumnLight = lightminF;
        if (wallColumnLight > lightmaxF) wallColumnLight = lightmaxF;
		lightcoefF += lightcoefFstep;
		
		wallColumnLightHalf = (prevWallColumnLight + wallColumnLight) >> 1;

		columnStoreData[0].light = (prevWallColumnLight + wallColumnLightHalf) >> 1;
		columnStoreData[1].light = wallColumnLightHalf;
		columnStoreData[2].light = (wallColumnLightHalf + wallColumnLight) >> 1;
		columnStoreData[3].light = wallColumnLight;
		columnStoreData+=4;

		prevWallColumnLight = wallColumnLight;
	}
	lightcoefFstep >>= 2;
	for (x-=3; x<=rightX; ++x) {
		prevWallColumnLight = (lightcoefF >> (16 - FIXEDTOSCALE)) - lightsub;
		if (prevWallColumnLight < lightminF) prevWallColumnLight = lightminF;
		if (prevWallColumnLight > lightmaxF) prevWallColumnLight = lightmaxF;
		lightcoefF += lightcoefFstep;
		columnStoreData->light = prevWallColumnLight;
		columnStoreData++;
	}
}

static void prepColumnStoreData(viswall_t *segl)
{
	Word x = segl->LeftX;
	const Word rightX = segl->RightX;

	int _scalefrac = segl->LeftScale;		// Init the scale fraction
	const int _scalestep = segl->ScaleStep;

    segloop_t *segdata = segloops;
    ColumnStore *columnStoreData = columnStoreArrayData;

	do {
        int scale = _scalefrac>>FIXEDTOSCALE;	// Current scaling factor
		if (scale >= 0x2000) {		// Too large?
			scale = 0x1fff;			// Fix the scale to maximum
		}

		columnStoreData->scale = scale;
		columnStoreData++;
		segdata->scale = scale;
		segdata->ceilingclipy = clipboundtop[x];	// Get the top y clip
		segdata->floorclipy = clipboundbottom[x];	// Get the bottom y clip
        segdata++;

        _scalefrac += _scalestep;		// Step to the next scale
	} while (++x<=rightX);

	prepColumnStoreDataLight(segl);

	columnStoreArrayData = columnStoreData;
}

/*
Culling idea that didn't work well so far (bugs, walls dissapear, not actually doing the clipping even)
Commenting out and will revisit in the future.
I am checking wall segment top bottom but those should be different depending if it's a mid wall or upper/lower walls separately.
Will even need to do this check at other places separately but mark for the renderer what to and not to render.
It's pretty hard to pull without destroying the visuals and actually not even gaining speed in most cases.

bool isSegWallOccluded(viswall_t *segl)
{

	const bool silsTop = segl->WallActions & (AC_TOPSIL|AC_NEWCEILING);
	const bool silsBottom = segl->WallActions & (AC_BOTTOMSIL|AC_NEWFLOOR);

	if (!silsTop && silsBottom) {
		const int xl = segl->LeftX;
		const int xr = segl->RightX;

		const int scaleLeft = (int)(segl->LeftScale >> FIXEDTOSCALE);
		const int scaleRight = (int)(segl->RightScale >> FIXEDTOSCALE);
		const int floorNewHeight = segl->floornewheight;
		const int ceilingNewHeight = segl->ceilingnewheight;

		const int bottomLeft = CenterY - ((scaleLeft * floorNewHeight)>>(HEIGHTBITS+SCALEBITS));
		const int bottomRight = CenterY - ((scaleRight * floorNewHeight)>>(HEIGHTBITS+SCALEBITS));

		if (bottomLeft < clipboundtop[xl] && bottomRight < clipboundtop[xr]) {
			const int topLeft = (CenterY-1) - ((scaleLeft * ceilingNewHeight)>>(HEIGHTBITS+SCALEBITS));
			const int topRight = (CenterY-1) - ((scaleRight * ceilingNewHeight)>>(HEIGHTBITS+SCALEBITS));

			if (segl->WallActions & AC_ADDSKY) {
				skyOnView = true;
			}

			if (topLeft > clipboundbottom[xl] && topRight > clipboundbottom[xr]) return true;
		}
	}
	return false;
}*/

void SegLoop(viswall_t *segl)
{
	if (optGraphics->renderer == RENDERER_DOOM) {
		if (optGraphics->depthShading >= DEPTH_SHADING_DITHERED) {
			if (segl->renderKind >= VW_MID) {
				prepColumnStoreDataUnlit(segl, true);
			} else {
				prepColumnStoreData(segl);
			}
		} else {
			prepColumnStoreDataUnlit(segl, false);
		}
	} else {
		if (segl->renderKind >= VW_FAR) {
			prepColumnStoreDataUnlit(segl, optGraphics->depthShading >= DEPTH_SHADING_DITHERED);
		} else {
			prepColumnStoreDataPoly(segl);
		}
	}

// Shall I add the floor?
    if (segl->WallActions & AC_ADDFLOOR) {
        SegLoopFloor(segl, CenterY);
    }

// Handle ceilings
    if (segl->WallActions & AC_ADDCEILING) {
        SegLoopCeiling(segl, CenterY);
    }

// Sprite clip sils
	{
		const bool silsTop = segl->WallActions & (AC_TOPSIL|AC_NEWCEILING);
		const bool silsBottom = segl->WallActions & (AC_BOTTOMSIL|AC_NEWFLOOR);

		if (silsTop && silsBottom) {
			SegLoopSpriteClipsTop(segl, CenterY);
			SegLoopSpriteClipsBottom(segl, CenterY);
		} else if (silsBottom) {
			SegLoopSpriteClipsBottom(segl, CenterY);
		} else if (silsTop) {
			SegLoopSpriteClipsTop(segl, CenterY);
		}
	}

// I can draw the sky right now!!
    if (!enableWireframeMode) {
        if (segl->WallActions & AC_ADDSKY) {
            skyOnView = true;
            if (optOther->sky==SKY_DEFAULT) {
                SegLoopSky(segl, CenterY);
            }
        }
    }
}

void PrepareSegLoop()
{
    Word i = 0;		// Init the vertical clipping records
    do {
        clipboundtop[i] = -1;		// Allow to the ceiling
        clipboundbottom[i] = ScreenHeight;	// Stop at the floor
    } while (++i<ScreenWidth);
}
