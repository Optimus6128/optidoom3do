#include "Doom.h"
#include <IntMath.h>
#include <operamath.h>

#define OPENMARK ((MAXSCREENHEIGHT-1)<<8)
#define CCB_ARRAY_PLANE_MAX MAXSCREENWIDTH

typedef struct {
    Word x1;
    Word x2;
} visspan_t;

Byte *PlaneSource;			/* Pointer to image of plane texture */
Fixed planey;		/* latched viewx / viewy for plane drawing */
Fixed basexscale,baseyscale;
Word PlaneDistance;
static Word PlaneHeight;

static visspan_t spandata[MAXSCREENHEIGHT];

static MyCCB CCBArrayPlane[CCB_ARRAY_PLANE_MAX];
static int CCBArrayPlaneCurrent = 0;

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
	MyCCB *CCBPtr;
	int i;

	CCBPtr = &CCBArrayPlane[0];
	for (i=0; i<CCB_ARRAY_PLANE_MAX; ++i) {
        CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDPLUT|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS;

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
	MyCCB *CCBPtr;
	int i;

	CCBPtr = &CCBArrayPlane[0];
	for (i=0; i<CCB_ARRAY_PLANE_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK;

        CCBPtr->ccb_PRE0 = 0x40000016;
        CCBPtr->ccb_PRE1 = 0x03FF1000;
        CCBPtr->ccb_SourcePtr = (CelData *)0;
        CCBPtr->ccb_HDY = 0<<20;
        CCBPtr->ccb_VDX = 0<<16;
        CCBPtr->ccb_VDY = 1<<16;

		++CCBPtr;
	}
}

static void initCCBarrayPlaneFlatVertical()
{
	MyCCB *CCBPtr;
	int i;

	CCBPtr = &CCBArrayPlane[0];
	for (i=0; i<CCB_ARRAY_PLANE_MAX; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK;

        CCBPtr->ccb_PRE0 = 0x40000016;
        CCBPtr->ccb_PRE1 = 0x03FF1000;
        CCBPtr->ccb_SourcePtr = (CelData *)0;
        CCBPtr->ccb_HDX = 0<<20;
        CCBPtr->ccb_VDX = 1<<16;
        CCBPtr->ccb_VDY = 0<<16;
		CCBPtr->ccb_HDDX = 0;
		CCBPtr->ccb_HDDY = 0;

		++CCBPtr;
	}
}

void initPlaneCELs()
{
	switch (optGraphics->planeQuality)
	{
		case PLANE_QUALITY_LO:
			if (optGraphics->depthShading == DEPTH_SHADING_ON) {
				initCCBarrayPlaneFlat();
			} else {
				initCCBarrayPlaneFlatVertical();
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
    MyCCB *spanCCBstart, *spanCCBend;

    spanCCBstart = &CCBArrayPlane[0];           // First span CEL of the plane segment
	spanCCBend = &CCBArrayPlane[xEnd];          // Last span CEL of the plane segment

	spanCCBend->ccb_Flags |= CCB_LAST;          // Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,(CCB*)spanCCBstart);     // Draw all the cels of a single plane in one shot

    spanCCBend->ccb_Flags ^= CCB_LAST;          // remember to flip off that CCB_LAST flag, since we don't reinit the flags for all spans every time
}

void drawCCBarrayPlaneVertical(MyCCB *columnCCBend)
{
    MyCCB *columnCCBstart;

	columnCCBstart = &CCBArrayPlane[0];     			// First column CEL of the plane segment

	columnCCBend->ccb_Flags |= CCB_LAST;                // Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,(CCB*)columnCCBstart);           // Draw all the cels of a single plane in one shot

    columnCCBend->ccb_Flags ^= CCB_LAST;                // remember to flip off that CCB_LAST flag, since we don't reinit the flags for all columns every time
}

void flushCCBarrayPlane()
{
	if (CCBArrayPlaneCurrent != 0) {
		drawCCBarrayPlane(CCBArrayPlaneCurrent - 1);
		CCBArrayPlaneCurrent = 0;
	}
}

static void MapPlane(Word y1, Word y2)
{
	angle_t	angle;
	Word distance;
	Fixed length;
	int light;
	Fixed xfrac,yfrac,xstep,ystep;
	Word x1;
    int y;

    MyCCB *CCBPtr;
    Byte *DestPtr;
    Word Count;
    int numCels;

    if (y1 > y2) return;
	numCels = y2 - y1 + 1;
	if (CCBArrayPlaneCurrent + numCels > CCB_ARRAY_PLANE_MAX) {
		flushCCBarrayPlane();
	}

    DestPtr = SpanPtr;
    CCBPtr = &CCBArrayPlane[CCBArrayPlaneCurrent];
    for (y=y1; y<=y2; ++y) {
        x1 = spandata[y].x1;
        Count = spandata[y].x2 - x1;
        distance = (yslope[y]*PlaneHeight)>>12;	/* Get the offset for the plane height */
        length = (distscale[x1]*distance)>>14;
        angle = (xtoviewangle[x1]+viewangle)>>ANGLETOFINESHIFT;

        xfrac = (((finecosine[angle]>>1)*length)>>4)+viewx;
        yfrac = planey - (((finesine[angle]>>1)*length)>>4);

        xstep = ((Fixed)distance*basexscale)>>4;
        ystep = ((Fixed)distance*baseyscale)>>4;

        light = lightcoef / (Fixed)distance - lightsub;
        if (light < lightmin) {
            light = lightmin;
        }
        if (light > lightmax) {
            light = lightmax;
        }


        spanDrawFunc(Count,xfrac,yfrac,xstep,ystep,DestPtr);

        CCBPtr->ccb_PRE1 = 0x3E005000|(Count-1);		/* Second preamble */
        CCBPtr->ccb_SourcePtr = (CelData *)DestPtr;	/* Save the source ptr */
        CCBPtr->ccb_PLUTPtr = PlaneSource;
        CCBPtr->ccb_XPos = x1<<16;		/* Set the x and y coord for start */
        CCBPtr->ccb_YPos = y<<16;
        CCBPtr->ccb_PIXC = LightTable[light>>LIGHTSCALESHIFT];			/* PIXC control */
        CCBPtr++;

        Count = (Count+3)&(~3);		/* Round to nearest longword */
        DestPtr += Count;
        SpanPtr = DestPtr;
    }
    CCBArrayPlaneCurrent += numCels;
}

static void MapPlaneUnshaded(Word y1, Word y2)
{
	angle_t	angle;
	Word distance;
	Fixed length;
	int light;
	Fixed xfrac,yfrac,xstep,ystep;
	Word x1;
    int y;

    MyCCB *CCBPtr;
    Byte *DestPtr;
    Word Count;
    int numCels;

    if (y1 > y2) return;
	numCels = y2 - y1 + 1;
	if (CCBArrayPlaneCurrent + numCels > CCB_ARRAY_PLANE_MAX) {
		flushCCBarrayPlane();
	}

    if (!optGraphics->depthShading) light = lightmin;
        else light = lightmax;

    light = LightTable[light>>LIGHTSCALESHIFT];

    DestPtr = SpanPtr;
    CCBPtr = &CCBArrayPlane[CCBArrayPlaneCurrent];
    for (y=y1; y<=y2; ++y) {
        x1 = spandata[y].x1;
        Count = spandata[y].x2 - x1;
        distance = (yslope[y]*PlaneHeight)>>12;	/* Get the offset for the plane height */
        length = (distscale[x1]*distance)>>14;
        angle = (xtoviewangle[x1]+viewangle)>>ANGLETOFINESHIFT;

        xfrac = (((finecosine[angle]>>1)*length)>>4)+viewx;
        yfrac = planey - (((finesine[angle]>>1)*length)>>4);

        xstep = ((Fixed)distance*basexscale)>>4;
        ystep = ((Fixed)distance*baseyscale)>>4;


        spanDrawFunc(Count,xfrac,yfrac,xstep,ystep,DestPtr);

        CCBPtr->ccb_PRE1 = 0x3E005000|(Count-1);		/* Second preamble */
        CCBPtr->ccb_SourcePtr = (CelData *)DestPtr;	/* Save the source ptr */
        CCBPtr->ccb_PLUTPtr = PlaneSource;
        CCBPtr->ccb_XPos = x1<<16;		/* Set the x and y coord for start */
        CCBPtr->ccb_YPos = y<<16;
        CCBPtr->ccb_PIXC = light;
        CCBPtr++;

        Count = (Count+3)&(~3);		/* Round to nearest longword */
        DestPtr += Count;
        SpanPtr = DestPtr;
    }
    CCBArrayPlaneCurrent += numCels;
}

static void MapPlaneFlat(Word y1, Word y2, Word color)
{
	Word distance;
	int light;
	Word x1, x2;
    int y;

    MyCCB *CCBPtr;
    void *colorValueAsPtr = (void*)color;
    int numCels;

    if (y1 > y2) return;
	numCels = y2 - y1 + 1;
	if (CCBArrayPlaneCurrent + numCels > CCB_ARRAY_PLANE_MAX) {
		flushCCBarrayPlane();
	}

    CCBPtr = &CCBArrayPlane[CCBArrayPlaneCurrent];
    for (y=y1; y<=y2; ++y) {
        x1 = spandata[y].x1;
        x2 = spandata[y].x2;
        distance = (yslope[y]*PlaneHeight)>>12;	/* Get the offset for the plane height */

        light = lightcoef / (Fixed)distance - lightsub;
        if (light < lightmin) {
            light = lightmin;
        }
        if (light > lightmax) {
            light = lightmax;
        }


        CCBPtr->ccb_PLUTPtr = colorValueAsPtr;
        CCBPtr->ccb_PIXC = LightTable[light>>LIGHTSCALESHIFT];
        CCBPtr->ccb_XPos = x1<<16;
        CCBPtr->ccb_YPos = y<<16;
        CCBPtr->ccb_HDX = (x2-x1)<<20;
        CCBPtr++;
    }
    CCBArrayPlaneCurrent += numCels;
}

static void MapPlaneAny(Word y1, Word y2, Word color)
{
    const bool lightQuality = (optGraphics->depthShading > 1);

    if (optGraphics->planeQuality > PLANE_QUALITY_LO) {
        if (lightQuality) {
            MapPlane(y1, y2);
        } else {
            MapPlaneUnshaded(y1, y2);
        }
    } else {
        MapPlaneFlat(y1, y2, color);
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

	const Word color = p->color;
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
	int topY, bottomY;
	Word *open = p->open;

	MyCCB *CCBPtr;
	void *colorValueAsPtr;

	Word light = p->PlaneLight;

    if (!optGraphics->depthShading) light = lightmins[light];
    light = LightTable[light>>LIGHTSCALESHIFT];

    colorValueAsPtr = (void*)p->color;


	x = p->minx;
	xEnd = p->maxx;

    if (xEnd < x) return;

	CCBPtr = CCBArrayPlane;
	do {
		const Word op = open[x];
		int length;
		topY = op >> 8;
		bottomY = op & 0xFF;

		length = bottomY - topY + 1;
		if (length < 1) continue;

        CCBPtr->ccb_PLUTPtr = colorValueAsPtr;
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
    if (optGraphics->planeQuality == PLANE_QUALITY_LO && optGraphics->depthShading != DEPTH_SHADING_ON) {
        DrawVisPlaneVertical(p);
    } else {
        DrawVisPlaneHorizontal(p);
    }
}
