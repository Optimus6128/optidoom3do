#include "Doom.h"
#include <IntMath.h>
#include <operamath.h>

#define OPENMARK ((MAXSCREENHEIGHT-1)<<8)
#define CCB_ARRAY_PLANE_MAX MAXSCREENWIDTH

typedef struct {
    Word x1;
    Word x2;
    Word distance;
    int light;
    Fixed xstep;
    Fixed ystep;
} visspan_t;

typedef struct {
    int start;
	int step;
} LightStepVars;

Byte *PlaneSource;			/* Pointer to image of plane texture */
Fixed planey;		/* latched viewx / viewy for plane drawing */
Fixed basexscale,baseyscale;
Word PlaneDistance;
static Word PlaneHeight;

static visspan_t spandata[MAXSCREENHEIGHT];

static BitmapCCB CCBArrayPlane[CCB_ARRAY_PLANE_MAX];
static int CCBArrayPlaneCurrent = 0;
static int CCBflagsCurrentAlteredIndex = 0;

static uint16 *coloredPlanePals = NULL;
static int currentVisplaneCount = 0;

static void *texPal;

static Word *LightTablePtr = LightTable;

static int visScrollX = 0;
static int visScrollY = 0;
static bool warpEffectOn = false;


/***************************

	Plane quality settings

****************************/

void (*spanDrawFunc)(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);


#include "bench.h"

/**********************************

	This is the basic primitive to draw horizontal lines quickly

**********************************/

static void initSpanDrawFunc(void)
{
    if (optGraphics->planeQuality==PLANE_QUALITY_HI)
        spanDrawFunc = DrawASpan;
    else
        spanDrawFunc = DrawASpanLo;
}

static void initCCBarrayPlane(void)
{
	BitmapCCB *CCBPtr;
	int i;

	CCBPtr = &CCBArrayPlane[0];
	for (i=0; i<CCB_ARRAY_PLANE_MAX; ++i) {
        CCBPtr->ccb_NextPtr = (BitmapCCB *)(sizeof(BitmapCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|/*CCB_NOBLK|*/CCB_PPABS;

        CCBPtr->ccb_PRE0 = 0x00000005;		// Preamble (Coded 8 bit)
        CCBPtr->ccb_HDX = 1<<20;
        CCBPtr->ccb_HDY = 0<<20;
        CCBPtr->ccb_VDX = 0<<16;
        CCBPtr->ccb_VDY = 1<<16;

		++CCBPtr;
	}
}

static void initCCBarrayPlaneFlat(void)
{
	BitmapCCB *CCBPtr;
	int i;

	CCBPtr = &CCBArrayPlane[0];
	for (i=0; i<CCB_ARRAY_PLANE_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (BitmapCCB *)(sizeof(BitmapCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND/*|CCB_NOBLK*/;

        CCBPtr->ccb_PRE0 = 0x40000016;
        CCBPtr->ccb_PRE1 = 0x03FF1000;
        CCBPtr->ccb_SourcePtr = (CelData *)0;
        CCBPtr->ccb_HDY = 0<<20;
        CCBPtr->ccb_VDX = 0<<16;
        CCBPtr->ccb_VDY = 1<<16;

		++CCBPtr;
	}
}

static void fillSpanArrayWithDitheredCheckerboard()
{
	int i;
	for (i=0; i<281; ++i) {
		*(SpanPtr + i) = i & 1;
	}
}

static void initCCBarrayPlaneFlatDithered()
{
	BitmapCCB *CCBPtr;
	int i;
	
	resetSpanPointer();
	fillSpanArrayWithDitheredCheckerboard();

	CCBPtr = &CCBArrayPlane[0];
	for (i=0; i<CCB_ARRAY_PLANE_MAX; ++i) {
        CCBPtr->ccb_NextPtr = (BitmapCCB *)(sizeof(BitmapCCB)-8);	// Create the next offset

	// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|/*CCB_NOBLK|*/CCB_PPABS;

        CCBPtr->ccb_HDX = 1<<20;
        CCBPtr->ccb_HDY = 0<<20;
        CCBPtr->ccb_VDX = 0<<16;
        CCBPtr->ccb_VDY = 1<<16;

	++CCBPtr;
	}
}

static void initCCBarrayPlaneFlatVertical()
{
	BitmapCCB *CCBPtr;
	int i;

	CCBPtr = &CCBArrayPlane[0];
	for (i=0; i<CCB_ARRAY_PLANE_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (BitmapCCB *)(sizeof(BitmapCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND/*|CCB_NOBLK*/;

        CCBPtr->ccb_PRE0 = 0x40000016;
        CCBPtr->ccb_PRE1 = 0x03FF1000;
        CCBPtr->ccb_SourcePtr = (CelData *)0;
        CCBPtr->ccb_HDX = 0<<20;
        CCBPtr->ccb_VDX = 1<<16;
        CCBPtr->ccb_VDY = 0<<16;

		++CCBPtr;
	}
}

void initPlaneCELs()
{
	if (!coloredPlanePals) {
		coloredPlanePals = (uint16*)AllocAPointer(32 * maxVisplanes * sizeof(uint16));
	}
	
	switch (optGraphics->planeQuality)
	{
		case PLANE_QUALITY_LO:
		{
			const Word depthShading = optGraphics->depthShading;
			if (depthShading == DEPTH_SHADING_ON) {
				initCCBarrayPlaneFlat();
			} else if (depthShading == DEPTH_SHADING_DITHERED) {
				initCCBarrayPlaneFlatDithered();
			} else {
				initCCBarrayPlaneFlatVertical();
			}
		}
		break;

		case PLANE_QUALITY_MED:
		case PLANE_QUALITY_HI:
			initCCBarrayPlane();
		break;
	}
	initSpanDrawFunc();
}

void drawCCBarrayPlane(Word xEnd)
{
    BitmapCCB *spanCCBstart, *spanCCBend;

    spanCCBstart = &CCBArrayPlane[0];           // First span CEL of the plane segment
	spanCCBend = &CCBArrayPlane[xEnd];          // Last span CEL of the plane segment
	dummyCCB->ccb_NextPtr = (CCB*)spanCCBstart;	// Start with dummy to reset HDDX and HDDY

	spanCCBend->ccb_Flags |= CCB_LAST;		// Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,dummyCCB);			// Draw all the cels of a single plane in one shot
    spanCCBend->ccb_Flags ^= CCB_LAST;		// remember to flip off that CCB_LAST flag, since we don't reinit the flags for all spans every time
}

void drawCCBarrayPlaneVertical(BitmapCCB *columnCCBend)
{
    BitmapCCB *columnCCBstart;

	columnCCBstart = &CCBArrayPlane[0];     		// First column CEL of the plane segment
	dummyCCB->ccb_NextPtr = (CCB*)columnCCBstart;	// Start with dummy to reset HDDX and HDDY

	columnCCBend->ccb_Flags |= CCB_LAST;		// Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem, dummyCCB);           	// Draw all the cels of a single plane in one shot
    columnCCBend->ccb_Flags ^= CCB_LAST;		// remember to flip off that CCB_LAST flag, since we don't reinit the flags for all columns every time
}

void flushCCBarrayPlane()
{
	if (CCBArrayPlaneCurrent != 0) {
		int i;
		drawCCBarrayPlane(CCBArrayPlaneCurrent - 1);
		CCBArrayPlaneCurrent = 0;

		for (i=0; i<CCBflagsCurrentAlteredIndex; ++i) {
			*CCBflagsAlteredIndexPtr[i] &= ~CCB_LDPLUT;
		}
		CCBflagsCurrentAlteredIndex = 0;
	}
}

static void MapPlane(Word y1, Word y2)
{
    BitmapCCB *CCBPtr;
	Byte *DestPtr;
    int y;
	Word numSpans;
	visspan_t *span;

    if (y1 > y2) return;

	numSpans = y2 - y1 + 1;

	if (CCBArrayPlaneCurrent + numSpans > CCB_ARRAY_PLANE_MAX) {
		flushCCBarrayPlane();
	}

	DestPtr = SpanPtr;
    CCBPtr = &CCBArrayPlane[CCBArrayPlaneCurrent];
	CCBPtr->ccb_Flags |= CCB_LDPLUT;
	CCBPtr->ccb_PLUTPtr = texPal;
	CCBflagsAlteredIndexPtr[CCBflagsCurrentAlteredIndex++] = &CCBPtr->ccb_Flags;
    CCBArrayPlaneCurrent += numSpans;

	span = &spandata[y1];
	y = y1 << 16;
	do {
        const Word x1 = span->x1;
        const Fixed length = (distscale[x1]*span->distance)>>14;
        const angle_t angle = (xtoviewangle[x1]+viewangle)>>ANGLETOFINESHIFT;

        const Fixed xfrac = (((finecosine[angle]>>1)*length)>>4) + viewx + visScrollX;
        const Fixed yfrac = planey - (((finesine[angle]>>1)*length)>>4) + visScrollY;

        Word Count = span->x2 - x1;

		if (warpEffectOn) {
			const int warpX = finecosine[((span->distance << 4) + (frameTime << 3)) & 8191] << 2;
			spanDrawFunc(Count,xfrac+warpX,yfrac, span->xstep, span->ystep, DestPtr);
		} else {
			spanDrawFunc(Count,xfrac,yfrac, span->xstep, span->ystep, DestPtr);
		}

        CCBPtr->ccb_PRE1 = 0x3E005000|(Count-1);		/* Second preamble */
        CCBPtr->ccb_SourcePtr = (CelData *)DestPtr;	/* Save the source ptr */
        CCBPtr->ccb_XPos = x1<<16;		/* Set the x and y coord for start */
        CCBPtr->ccb_YPos = y;
        CCBPtr->ccb_PIXC = span->light;			/* PIXC control */
        CCBPtr++;

		y+=1<<16;
		++span;

        Count = (Count+3)&(~3);		/* Round to nearest longword */
        DestPtr += Count;
        SpanPtr = DestPtr;
    } while(--numSpans != 0);
}

static void MapPlaneUnshaded(Word y1, Word y2)
{
    BitmapCCB *CCBPtr;
    Byte *DestPtr;
    int y, light;
	Word numSpans;
	visspan_t *span;

    if (y1 > y2) return;

	numSpans = y2 - y1 + 1;

	if (CCBArrayPlaneCurrent + numSpans > CCB_ARRAY_PLANE_MAX) {
		flushCCBarrayPlane();
	}

    DestPtr = SpanPtr;
    CCBPtr = &CCBArrayPlane[CCBArrayPlaneCurrent];
	CCBPtr->ccb_Flags |= CCB_LDPLUT;
	CCBPtr->ccb_PLUTPtr = texPal;
	CCBflagsAlteredIndexPtr[CCBflagsCurrentAlteredIndex++] = &CCBPtr->ccb_Flags;
	CCBArrayPlaneCurrent += numSpans;

    if (!optGraphics->depthShading) light = lightmin;
        else light = lightmax;
    light = LightTablePtr[light>>LIGHTSCALESHIFT];

	span = &spandata[y1];
	y = y1 << 16;
    do {
        const Word x1 = span->x1;
        const Fixed length = (distscale[x1]*span->distance)>>14;
        const angle_t angle = (xtoviewangle[x1]+viewangle)>>ANGLETOFINESHIFT;

        const Fixed xfrac = (((finecosine[angle]>>1)*length)>>4) + viewx + visScrollX;
        const Fixed yfrac = planey - (((finesine[angle]>>1)*length)>>4) + visScrollY;

        Word Count = span->x2 - x1;

		if (warpEffectOn) {
			const int warpX = finecosine[((span->distance << 4) + (frameTime << 3)) & 8191] << 2;
			spanDrawFunc(Count,xfrac+warpX,yfrac, span->xstep, span->ystep, DestPtr);
		} else {
			spanDrawFunc(Count,xfrac,yfrac, span->xstep, span->ystep, DestPtr);
		}

        CCBPtr->ccb_PRE1 = 0x3E005000|(Count-1);		/* Second preamble */
        CCBPtr->ccb_SourcePtr = (CelData *)DestPtr;	/* Save the source ptr */
        CCBPtr->ccb_XPos = x1<<16;		/* Set the x and y coord for start */
        CCBPtr->ccb_YPos = y;
        CCBPtr->ccb_PIXC = light;
        CCBPtr++;

		y+=1<<16;
		++span;

        Count = (Count+3)&(~3);		/* Round to nearest longword */
        DestPtr += Count;
        SpanPtr = DestPtr;
    } while(--numSpans != 0);
}

static void MapPlaneFlat(Word y1, Word y2, Word color)
{
    BitmapCCB *CCBPtr;
    int numCels;
    int y;

    if (y1 > y2) return;
	numCels = y2 - y1 + 1;
	if (CCBArrayPlaneCurrent + numCels > CCB_ARRAY_PLANE_MAX) {
		flushCCBarrayPlane();
	}

    CCBPtr = &CCBArrayPlane[CCBArrayPlaneCurrent];
    for (y=y1; y<=y2; ++y) {
        const Word x1 = spandata[y].x1;
        const Word x2 = spandata[y].x2;
		const int light = spandata[y].light;

        CCBPtr->ccb_PLUTPtr = (void*)color;
        CCBPtr->ccb_PIXC = light;
        CCBPtr->ccb_XPos = x1<<16;
        CCBPtr->ccb_YPos = y<<16;
        CCBPtr->ccb_HDX = (x2-x1)<<20;
        CCBPtr++;
    }
    CCBArrayPlaneCurrent += numCels;
}

static void MapPlaneFlatDithered(Word y1, Word y2, const Word *color)
{
    BitmapCCB *CCBPtr;
    int numCels;
    int y;

    if (y1 > y2) return;
	numCels = y2 - y1 + 1;
	if (CCBArrayPlaneCurrent + numCels > CCB_ARRAY_PLANE_MAX) {
		flushCCBarrayPlane();
	}

    CCBPtr = &CCBArrayPlane[CCBArrayPlaneCurrent];
	CCBPtr->ccb_Flags |= CCB_LDPLUT;
	CCBPtr->ccb_PLUTPtr = (void*)color;
	CCBflagsAlteredIndexPtr[CCBflagsCurrentAlteredIndex++] = &CCBPtr->ccb_Flags;
    for (y=y1; y<=y2; ++y) {
		const Word x1 = spandata[y].x1;
		const Word distance = spandata[y].distance;
		int light = spandata[y].light;
        const Word Count = spandata[y].x2 - x1;
        const int poffX = (y + x1) & 1;
        const Word pixc1 = LightTablePtr[light];

		CCBPtr->ccb_PRE0 = 0x00000005 | (poffX << 24);	// Preamble (Coded 8 bit)
        CCBPtr->ccb_PRE1 = 0x3E005000|(Count+poffX-1);		/* Second preamble */
        CCBPtr->ccb_SourcePtr = (CelData *)SpanPtr;	/* Save the source ptr */
        CCBPtr->ccb_XPos = x1<<16;		/* Set the x and y coord for start */
        CCBPtr->ccb_YPos = y<<16;

        if ((distance & 65535) > 32767) {	// that kinda works, not sure of the full range yet
        	CCBPtr->ccb_PIXC = (pixc1 << 16) | pixc1;
        } else {
			if (--light < 0) light = 0;
        	CCBPtr->ccb_PIXC = (pixc1 << 16) | LightTablePtr[light];
        }
        CCBPtr++;
    }
    CCBArrayPlaneCurrent += numCels;
}

static void MapPlaneAny(Word y1, Word y2, const Word *color)
{
	const Word depthShading = optGraphics->depthShading;

	if (optGraphics->planeQuality > PLANE_QUALITY_LO) {
		if (depthShading >= DEPTH_SHADING_DITHERED) {
			MapPlane(y1, y2);
		} else {
			MapPlaneUnshaded(y1, y2);
		}
    } else {
    	if (depthShading == DEPTH_SHADING_DITHERED) {
			MapPlaneFlatDithered(y1, y2, color);
		} else {
			MapPlaneFlat(y1, y2, *color);
    	}
    }
}

static LightStepVars* getLightInterpolationVars(int y1, int y2)
{
	static LightStepVars lsv;

	const Word distance1 = (yslope[y1]*PlaneHeight)>>12;
	const Word distance2 = (yslope[y2]*PlaneHeight)>>12;

	const int light1 = lightcoef / (Fixed)distance1 - lightsub;
	const int light2 = lightcoef / (Fixed)distance2 - lightsub;

	const int steps = y2 - y1 + 1;

	lsv.start = light1 << 16;
	lsv.step = ((light2 - light1) << 16) / steps;

	return &lsv;
}

static void initVisplaneSpanDataTextured(visplane_t *p)
{
	int y = p->miny;
	const int yMax = p->maxy;

	LightStepVars *lsv = getLightInterpolationVars(y, yMax);
	int lightPos = lsv->start;
	const int lightStep = lsv->step;

	for (; y<=yMax; ++y) {
		const Word distance = (yslope[y]*PlaneHeight)>>12;
		int light = lightPos >> 16;

        if (light < lightmin) {
            light = lightmin;
        } else if (light > lightmax) {
            light = lightmax;
        }

        spandata[y].light = LightTablePtr[light>>LIGHTSCALESHIFT];
		spandata[y].distance = distance;
		spandata[y].xstep = ((Fixed)distance*basexscale)>>4;
		spandata[y].ystep = ((Fixed)distance*baseyscale)>>4;
		
		lightPos += lightStep;
	}
}

static void initVisplaneSpanDataTexturedUnshaded(visplane_t *p)
{
	int y;
	const int yMax = p->maxy;
	for (y=p->miny; y<=yMax; ++y) {
		const Word distance = (yslope[y]*PlaneHeight)>>12;

		spandata[y].distance = distance;
		spandata[y].xstep = ((Fixed)distance*basexscale)>>4;
		spandata[y].ystep = ((Fixed)distance*baseyscale)>>4;
	}
}

static void initVisplaneSpanDataFlat(visplane_t *p)
{
	int y = p->miny;
	const int yMax = p->maxy;

	LightStepVars *lsv = getLightInterpolationVars(y, yMax);
	int lightPos = lsv->start;
	const int lightStep = lsv->step;

	for (; y<=yMax; ++y) {
		int light = lightPos >> 16;

        if (light < lightmin) {
            light = lightmin;
        } else if (light > lightmax) {
            light = lightmax;
        }

        spandata[y].light = LightTablePtr[light>>LIGHTSCALESHIFT];

		lightPos += lightStep;
	}
}

static void initVisplaneSpanDataFlatDithered(visplane_t *p)
{
	int y = p->miny;
	const int yMax = p->maxy;

	LightStepVars *lsv = getLightInterpolationVars(y, yMax);
	int lightPos = lsv->start;
	const int lightStep = lsv->step;

	for (; y<=yMax; ++y) {
		const Word distance = (yslope[y]*PlaneHeight)>>12;
		int light = lightPos >> 16;

        if (light < lightmin) {
            light = lightmin;
        } else if (light > lightmax) {
            light = lightmax;
        }

        spandata[y].light = light>>LIGHTSCALESHIFT;
		spandata[y].distance = distance;

		lightPos += lightStep;
	}
}

static void initVisplaneSpanData(visplane_t *p)
{
	const Word planeQuality = optGraphics->planeQuality;
	const Word depthShading = optGraphics->depthShading;

	if (planeQuality > PLANE_QUALITY_LO) {
		if (depthShading >= DEPTH_SHADING_DITHERED) {
			initVisplaneSpanDataTextured(p);
		} else {
			initVisplaneSpanDataTexturedUnshaded(p);
		}
	} else {
		if (depthShading == DEPTH_SHADING_DITHERED) {
			initVisplaneSpanDataFlatDithered(p);
		} else {
			initVisplaneSpanDataFlat(p);
		}
	}
}

/**********************************

	Draw a plane by scanning the open records.
	The open records are an array of top and bottom Y's for
	a graphical plane. I traverse the array to find out the horizontal
	spans I need to draw. This is a bottleneck routine.

**********************************/

void DrawVisPlaneHorizontal(visplane_t *p)
{
	register Word x;
	Word stop;
	Word oldtop;
	Word markY;
	register Word *open;

	const Word *color = &p->color;
	const Word special = p->special;

	PlaneSource = (Byte *)*p->PicHandle;	/* Get the base shape index */

	x = p->height;
	if ((int)x<0) {
		x = -x;
	}
	PlaneHeight = x;

	stop = p->PlaneLight;
	lightmin = lightmins[stop];
	lightmax = stop;
	lightsub = lightsubs[stop];
	lightcoef = planelightcoef[stop];

	if (p->color==0) {
		texPal = PlaneSource;
	} else {
		texPal = &coloredPlanePals[currentVisplaneCount << 5];
		initColoredPals((uint16*)PlaneSource, texPal, 32, p->color);
		if (++currentVisplaneCount == maxVisplanes) currentVisplaneCount = 0;
	}

	if (special & SEC_SPEC_FOG) {
		LightTablePtr = LightTableFog;
	} else {
		LightTablePtr = LightTable;
	}

	visScrollX = 0;
	visScrollY = 0;
	warpEffectOn = false;
	if (special & SEC_SPEC_SCROLL_OR_WARP) {
		const int dirs = (special & SEC_SPEC_DIRS) >> 7;
		if (special & SEC_SPEC_SCROLL) {
			const int scrollVel = frameTime << 12;
			switch(dirs) {
				case 0:
					visScrollX = -scrollVel;
					break;
				case 1:
					visScrollX = scrollVel;
					break;
				case 2:
					visScrollY = scrollVel;
					break;
				case 3:
					visScrollY = -scrollVel;
					break;
			}
		} else {
			warpEffectOn = (p->isFloor && dirs==1) || (!p->isFloor && dirs==2) || (dirs == 3);
		}
	}

	initVisplaneSpanData(p);

	stop = p->maxx+1;	/* Maximum x coord */
	x = p->minx;		/* Starting x */
	open = p->open;		/* Init the pointer to the open Y's */
	oldtop = OPENMARK;	/* Get the top and bottom Y's */
	open[stop] = oldtop;	/* Set posts to stop drawing */

	do {
		Word newtop;
		newtop = open[x];		/* Fetch the NEW top and bottom */
		if (oldtop!=newtop) {
			Word PrevTopY,NewTopY;		/* Previous and dest Y coords for top line */
			Word PrevBottomY,NewBottomY;	/* Previous and dest Y coords for bottom line */
			PrevTopY = oldtop>>8;		/* Starting Y coords */
			PrevBottomY = oldtop&0xFF;
			NewTopY = newtop>>8;
			NewBottomY = newtop&0xff;

			/* For lines on the top, check if the entry is going down */

			if (PrevTopY < NewTopY && PrevTopY<=PrevBottomY) {	/* Valid? */
				register Word Count;
				Count = PrevBottomY+1;	/* Convert to < */
				if (NewTopY<Count) {	/* Use the lower */
					Count = NewTopY;	/* This is smaller */
				}
				markY = PrevTopY;
				do {
                    spandata[PrevTopY].x2 = x;
				} while (++PrevTopY<Count);
                MapPlaneAny(markY, Count-1, color);
			}
			if (NewTopY < PrevTopY && NewTopY<=NewBottomY) {
				register Word Count;
				Count = NewBottomY+1;
				if (PrevTopY<Count) {
					Count = PrevTopY;
				}
				do {
					spandata[NewTopY].x1 = x;	/* Mark the starting x's */
				} while (++NewTopY<Count);
			}

			if (PrevBottomY > NewBottomY && PrevBottomY>=PrevTopY) {
				register int Count;
				Count = PrevTopY-1;
				if (Count<(int)NewBottomY) {
					Count = NewBottomY;
				}
                markY = PrevBottomY;
				do {
                    spandata[PrevBottomY].x2 = x;
				} while ((int)--PrevBottomY>Count);
                MapPlaneAny(Count+1, markY, color);
			}
			if (NewBottomY > PrevBottomY && NewBottomY>=NewTopY) {
				register int Count;
				Count = NewTopY-1;
				if (Count<(int)PrevBottomY) {
					Count = PrevBottomY;
				}
				do {
					spandata[NewBottomY].x1 = x;		/* Mark the starting x's */
				} while ((int)--NewBottomY>Count);
			}
			oldtop=newtop;
		}
	} while (++x<=stop);
}


void DrawVisPlaneVertical(visplane_t *p)
{
	int x, xEnd;
	Word *open = p->open;

	BitmapCCB *CCBPtr;
	const Word color = p->color;

	Word light = p->PlaneLight;

    if (!optGraphics->depthShading) light = lightmins[light];
    light = LightTablePtr[light>>LIGHTSCALESHIFT];


	x = p->minx;
	xEnd = p->maxx;

    if (xEnd < x) return;

	CCBPtr = CCBArrayPlane;
	do {
		const Word op = open[x];
		int length;
		const int topY = op >> 8;
		const int bottomY = op & 0xFF;

		length = bottomY - topY + 1;
		if (length < 1) continue;

        CCBPtr->ccb_PLUTPtr = (void*)color;
        CCBPtr->ccb_PIXC = light;
        CCBPtr->ccb_XPos = x<<16;
        CCBPtr->ccb_YPos = topY<<16;
        CCBPtr->ccb_HDY = length<<20;

        ++CCBPtr;
	} while (++x<=xEnd);

	if (CCBPtr != CCBArrayPlane)
        drawCCBarrayPlaneVertical(--CCBPtr);
}

void DrawVisPlane(visplane_t *p)
{
	const Word planeQuality = optGraphics->planeQuality;
	const Word depthShading = optGraphics->depthShading;

    if (planeQuality == PLANE_QUALITY_LO && depthShading < DEPTH_SHADING_DITHERED) {
        DrawVisPlaneVertical(p);
    } else {
        DrawVisPlaneHorizontal(p);
    }
}
