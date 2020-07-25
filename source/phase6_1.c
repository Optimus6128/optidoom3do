#include "Doom.h"
#include <IntMath.h>

#define OPENMARK ((MAXSCREENHEIGHT-1)<<8)

static MyCCB CCBArraySky[MAXSCREENWIDTH];       // Array of CCB struct for the sky columns

typedef struct {
	int scale;
	int ceilingclipy;
	int floorclipy;
} segloop_t;


static Word clipboundtop[MAXSCREENWIDTH];		// Bounds top y for vertical clipping
static Word clipboundbottom[MAXSCREENWIDTH];	// Bounds bottom y for vertical clipping

segloop_t segloops[MAXSCREENWIDTH];

bool skyOnView = false;                         // marker to know if at one frame sky was visible or not


void initCCBarraySky(void)
{
	MyCCB *CCBPtr;
	int i;

	const int skyScale = getSkyScale();

	CCBPtr = CCBArraySky;
	for (i=0; i<MAXSCREENWIDTH; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
                            CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS;	// ccb_flags

        if (i==0) CCBPtr->ccb_Flags |= CCB_LDPLUT;  // First CEL column will set the palette for the rest

        CCBPtr->ccb_PRE0 = 0x03;
        CCBPtr->ccb_PRE1 = 0x3E005000|(128-1);  // Project the pixels
        CCBPtr->ccb_YPos = 0<<16;
        CCBPtr->ccb_HDX = 0<<20;		        // Convert 6 bit frac to CCB scale
        CCBPtr->ccb_HDY = skyScale;             // Video stretch factor
        CCBPtr->ccb_VDX = 1<<16;
        CCBPtr->ccb_VDY = 0<<16;
		CCBPtr->ccb_HDDX = 0;
		CCBPtr->ccb_HDDY = 0;
        CCBPtr->ccb_PIXC = 0x1F00;              // PIXC control

		++CCBPtr;
	}
}


/**********************************

	Given a span of pixels, see if it is already defined
	in a record somewhere. If it is, then merge it otherwise
	make a new plane definition.

**********************************/

static visplane_t *FindPlane(visplane_t *check, viswall_t *segl, int start)
{
	Fixed height = segl->floorheight;
	void **PicHandle = segl->FloorPic;
	int stop = segl->RightX;
	Word Light = segl->seglightlevel;

	++check;		/* Automatically skip to the next plane */
	if (check<lastvisplane) {
		do {
			if (height == check->height &&		/* Same plane as before? */
				PicHandle == check->PicHandle &&
				Light == check->PlaneLight &&
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
	check->height = height;		/* Init all the vars in the visplane */
	check->PicHandle = PicHandle;
	check->minx = start;
	check->maxx = stop;
	check->PlaneLight = Light;		/* Set the light level */


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
	int scale;

	visplane_t *FloorPlane;
	int top, bottom;
	int ceilingclipy, floorclipy;

	segloop_t *segdata = segloops;

	FloorPlane = visplanes;		// Reset the visplane pointers

	x = segl->LeftX;
	do {
		scale = segdata->scale;
		ceilingclipy = segdata->ceilingclipy;
		floorclipy = segdata->floorclipy;

        top = screenCenterY - ((scale * segl->floorheight)>>(HEIGHTBITS+SCALEBITS));	// Y coord of top of floor
        if (top <= ceilingclipy) {
            top = ceilingclipy+1;		// Clip the top of floor to the bottom of the visible area
        }
        bottom = floorclipy-1;		// Draw to the bottom of the screen
        if (top <= bottom) {		// Valid span?
            if (FloorPlane->open[x] != OPENMARK) {	// Not already covered?
                FloorPlane = FindPlane(FloorPlane, segl, x);
            }
            if (top) {
                --top;
            }
            FloorPlane->open[x] = (top<<8)+bottom;	// Set the new vertical span
        }
        segdata++;
	} while (++x<=segl->RightX);
}

static void SegLoopCeiling(viswall_t *segl, Word screenCenterY)
{
	Word x;
	int scale;

	visplane_t *CeilingPlane;
	int top, bottom;
	int ceilingclipy, floorclipy;

	segloop_t *segdata = segloops;

	CeilingPlane = visplanes;		// Reset the visplane pointers

	// Ugly hack for the case FindPlane expects segl, but reads always floor (to not pass too many arguments as before and also not duplicate)
	segl->floorheight = segl->ceilingheight;
	segl->FloorPic = segl->CeilingPic;

	x = segl->LeftX;
	do {
		scale = segdata->scale;
		ceilingclipy = segdata->ceilingclipy;
		floorclipy = segdata->floorclipy;

        top = ceilingclipy+1;		// Start from the ceiling
        bottom = (screenCenterY-1) - ((scale * segl->ceilingheight)>>(HEIGHTBITS+SCALEBITS));	// Bottom of the height
        if (bottom >= floorclipy) {		// Clip the bottom?
            bottom = floorclipy-1;
        }
        if (top <= bottom) {
            if (CeilingPlane->open[x] != OPENMARK) {		// Already in use?
                CeilingPlane = FindPlane(CeilingPlane, segl, x);
            }
            if (top) {
                --top;
            }
            CeilingPlane->open[x] = (top<<8)+bottom;		// Set the vertical span
        }
        segdata++;
	} while (++x<=segl->RightX);
}

static void SegLoopSpriteClipsBottom(viswall_t *segl, Word screenCenterY)
{
    Word ActionBits = segl->WallActions;
	segloop_t *segdata = segloops;

	Word x = segl->LeftX;
	do {
        int low = screenCenterY - ((segdata->scale * segl->floornewheight)>>(HEIGHTBITS+SCALEBITS));
        int floorclipy = segdata->floorclipy;

        if (low > floorclipy) {
            low = floorclipy;
        }
        if (low < 0) {
            low = 0;
        }
        if (ActionBits & AC_BOTTOMSIL) {
            segl->BottomSil[x] = low;
        }
        if (ActionBits & AC_NEWFLOOR) {
            clipboundbottom[x] = low;
        }
		segdata++;
	} while (++x<=segl->RightX);
}

static void SegLoopSpriteClipsTop(viswall_t *segl, Word screenCenterY)
{
    Word ActionBits = segl->WallActions;
	segloop_t *segdata = segloops;

	Word x = segl->LeftX;
	do {
        int high = (screenCenterY-1) - ((segdata->scale * segl->ceilingnewheight)>>(HEIGHTBITS+SCALEBITS));
        int ceilingclipy = segdata->ceilingclipy;

        if (high < ceilingclipy) {
            high = ceilingclipy;
        }
        if (high > (int)ScreenHeight-1) {
            high = ScreenHeight-1;
        }
        if (ActionBits & AC_TOPSIL) {
            segl->TopSil[x] = high+1;
        }
        if (ActionBits & AC_NEWCEILING) {
            clipboundtop[x] = high;
        }
		segdata++;
	} while (++x<=segl->RightX);
}

static void SegLoopSky(viswall_t *segl, Word screenCenterY)
{
	int scale;
	int ceilingclipy, floorclipy;
	int bottom;

	MyCCB *CCBPtr = &CCBArraySky[0];
    Byte *Source = (Byte *)(*SkyTexture->data);

	segloop_t *segdata = segloops;

    Word x = segl->LeftX;
    do {
		scale = segdata->scale;
		ceilingclipy = segdata->ceilingclipy;
		floorclipy = segdata->floorclipy;

        bottom = screenCenterY - ((scale * segl->ceilingheight)>>(HEIGHTBITS+SCALEBITS));
        if (bottom > floorclipy) {
            bottom = floorclipy;
        }
        if ((ceilingclipy+1) < bottom) {		// Valid?
            CCBPtr->ccb_XPos = x<<16;                               // Set the x and y coord for start
            CCBPtr->ccb_SourcePtr = (CelData *)&Source[((((xtoviewangle[x]+viewangle)>>ANGLETOSKYSHIFT)&0xFF)<<6) + 32];	// Get the source ptr
            ++CCBPtr;
        }
        segdata++;
	} while (++x<=segl->RightX);


	if (CCBPtr != &CCBArraySky[0]) {
        CCBArraySky[0].ccb_PLUTPtr = Source;    // plut pointer only for first element
        drawCCBarray(--CCBPtr, CCBArraySky);
	}
}

void SegLoop(viswall_t *segl)
{
	Word x;

	int _scalefrac = segl->LeftScale;		// Init the scale fraction
	const int _scalestep = segl->ScaleStep;

	Word ActionBits = segl->WallActions;

    segloop_t *segdata = segloops;

    int *scaleData = scaleArrayData;

	x = segl->LeftX;
	do {
        int scale = _scalefrac>>FIXEDTOSCALE;	// Current scaling factor
		if (scale >= 0x2000) {		// Too large?
			scale = 0x1fff;			// Fix the scale to maximum
		}
		*scaleData++ = scale;
		segdata->scale = scale;
		segdata->ceilingclipy = clipboundtop[x];	// Get the top y clip
		segdata->floorclipy = clipboundbottom[x];	// Get the bottom y clip
        segdata++;

        _scalefrac += _scalestep;		// Step to the next scale
	} while (++x<=segl->RightX);
	scaleArrayData = scaleData;


// Shall I add the floor?
    if (ActionBits & AC_ADDFLOOR) {
        SegLoopFloor(segl, CenterY);
    }

// Handle ceilings
    if (ActionBits & AC_ADDCEILING) {
        SegLoopCeiling(segl, CenterY);
    }

// Sprite clip sils
    if (ActionBits & (AC_BOTTOMSIL|AC_NEWFLOOR)) {
        SegLoopSpriteClipsBottom(segl, CenterY);
    }

    if (ActionBits & (AC_TOPSIL|AC_NEWCEILING)) {
        SegLoopSpriteClipsTop(segl, CenterY);
    }

// I can draw the sky right now!!
    if (opt_gimmicks != GIMMICKS_WIREFRAME) {
        if (ActionBits & AC_ADDSKY) {
            skyOnView = true;
            if (opt_sky==SKY_DEFAULT) {
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
