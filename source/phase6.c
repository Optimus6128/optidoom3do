#include "Doom.h"
#include <IntMath.h>

#define OPENMARK ((MAXSCREENHEIGHT-1)<<8)

/**********************************

	This code will draw all the VERTICAL walls for
	a screen.

	Clip values are the solid pixel bounding the range
	floorclip starts out ScreenHeight
	ceilingclip starts out -1
	clipbounds[] = (ceilingclip+1)<<8 + floorclip

**********************************/

typedef struct {
    int scale;
    Word column;
    Word light;
} viscol_t;

typedef struct {
	int scale;
	int ceilingclipy;
	int floorclipy;
} segloop_t;


static MyCCB CCBArrayWall[MAXSCREENWIDTH];		// Array of CCB structs for a single wall segment
static MyCCB CCBArrayWallFlat[MAXSCREENWIDTH];	// Array of CCB structs for a single wall segment (untextured)
static MyCCB CCBArraySky[MAXSCREENWIDTH];       // Array of CCB struct for the sky columns

static Word clipboundtop[MAXSCREENWIDTH];		// Bounds top y for vertical clipping
static Word clipboundbottom[MAXSCREENWIDTH];	// Bounds bottom y for vertical clipping

static int scaleArray[MAXSCREENWIDTH * 16];     // MAXSCREENWIDTH * 16 should be more than enough
static int *scaleArrayPtr[MAXWALLCMDS];         // start pointers in scaleArray for each individual line segment
static int *scaleArrayData;                     // pointer for new line segment
static int scaleArrayIndex;                     // index for new line segment



static drawtex_t drawtex;

Word tx_x;			/* Screen x coord being drawn */
int tx_scale;		/* True scale value 0-0x7FFF */
Word tx_texturecolumn;	/* Column offset into source image */

viscol_t viscols[MAXSCREENWIDTH];
segloop_t segloops[MAXSCREENWIDTH];

bool skyOn = false;

/***************************

	Wall quality settings

****************************/

enum
{
    WALL_QUALITY_LO = 0,
    WALL_QUALITY_MED = 1,
    WALL_QUALITY_HI = 2
};

Word wallQuality = WALL_QUALITY_HI;         // 0=LO(Untextured), 1=MED, 2=HI
Word columnWidth;                           // column width in pixels (1 for fullRes, 2 for halfRes)

Word depthShadingOption = 2;     // 0=OFF(DARK), 1=OFF(BRIGHT), 2=ON
Word thingsShadingOption = 0;    // 0=OFF, 1=ON
Word rendererOption = 0;         // 0=DOOM, 1=LIGOLEAST, 2=LIGOMOST

Word skyType = SKY_DEFAULT;


/**********************************

	Calculate texturecolumn and iscale for the rendertexture routine

**********************************/

void initCCBarrayWall(void)
{
	MyCCB *CCBPtr;
	int i;

    columnWidth = 1;
    if (wallQuality==WALL_QUALITY_MED)
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

	CCBPtr = CCBArrayWallFlat;
	for (i=0; i<MAXSCREENWIDTH; ++i) {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	// Create the next offset

		// Set all the defaults
        CCBPtr->ccb_Flags = CCB_LDSIZE|CCB_LDPRS|CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_USEAV|CCB_ACSC|CCB_ALSC;	/* ccb_flags */

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

void initCCBarraySky(void)
{
	MyCCB *CCBPtr;
	int i;

	const int skyScale = getSkyScale(ScreenSizeOption);

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

void drawCCBarrayAny(MyCCB* lastCCB, Byte *source, MyCCB *CCBArrayPtr)
{
    MyCCB *columnCCBstart, *columnCCBend;

	columnCCBstart = CCBArrayPtr;                // First column CEL of the wall segment
	columnCCBend = lastCCB;                      // Last column CEL of the wall segment

	columnCCBstart->ccb_PLUTPtr = source;        // Don't forget to set up the palette pointer, only for the first column
	columnCCBend->ccb_Flags |= CCB_LAST;         // Mark last colume CEL as the last one in the linked list
    DrawCels(VideoItem,(CCB*)columnCCBstart);    // Draw all the cels of a single wall in one shot
    columnCCBend->ccb_Flags ^= CCB_LAST;         // remember to flip off that CCB_LAST flag, since we don't reinit the flags for all columns every time
}

void drawCCBarrayAnyFlat(MyCCB* lastCCB, MyCCB *CCBArrayPtr)
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
        CCBPtr->ccb_HDY = vc->scale<<11;
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control

        CCBPtr++;
        vc++;
    }while (++xPos <= tex->xEnd);

    // Call for the final render of the linked list of all the column cels of the single wall segment
    drawCCBarrayAny(--CCBPtr, tex->data, CCBArrayWall);
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
        CCBPtr->ccb_HDY = vc->scale<<11;
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control

        CCBPtr++;
        vc++;
        xPos+=2;
    }while (xPos <= tex->xEnd);

    // Call for the final render of the linked list of all the column cels of the single wall segment
    drawCCBarrayAny(--CCBPtr, tex->data, CCBArrayWall);
}

static void DrawWallSegmentFlat(drawtex_t *tex, Word screenCenterY)
{
    int xPos = tex->xStart;
	int top;
	Word run;
	viscol_t *vc;

	MyCCB *CCBPtr;
	Byte *plutPtr;

	if (xPos > tex->xEnd) return;

	run = (tex->topheight-tex->bottomheight)>>HEIGHTBITS;	// Source image height
	if ((int)run<=0) {		// Invalid?
		return;
	}

	plutPtr = (Byte*)(((Word*)tex->data)[0] << 16);

    CCBPtr = CCBArrayWallFlat;
    vc = viscols;
    do {
        top = screenCenterY - ((vc->scale*tex->topheight) >> (HEIGHTBITS+SCALEBITS));	// Screen Y

        CCBPtr->ccb_PLUTPtr = plutPtr;
        CCBPtr->ccb_XPos = xPos << 16;
        CCBPtr->ccb_YPos = (top<<16) + 0xFF00;
        CCBPtr->ccb_HDY = (run * vc->scale) << 11;
        CCBPtr->ccb_PIXC = LightTable[vc->light>>LIGHTSCALESHIFT];		// PIXC control

        CCBPtr++;
        vc++;
    }while (++xPos <= tex->xEnd);

    // Call for the final render of the linked list of all the column cels of the single wall segment
    drawCCBarrayAnyFlat(--CCBPtr, CCBArrayWallFlat);
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

static void DrawSegFull(viswall_t *segl, int *scaleData)
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

static void DrawSegFullUnshaded(viswall_t *segl, int *scaleData)
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
    if (!depthShadingOption) textureLight = lightmins[i];
        else textureLight = i;

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

static void DrawSegFullFlat(viswall_t *segl, int *scaleData)
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

static void DrawSegHalf(viswall_t *segl, int *scaleData)
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

static void DrawSegHalfUnshaded(viswall_t *segl, int *scaleData)
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
    if (!depthShadingOption) textureLight = lightmins[i];
        else textureLight = i;

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
        j = ScreenWidth/8;
        do {
            set[0] = i;
            set[1] = i;
            set[2] = i;
            set[3] = i;
            set[4] = i;
            set[5] = i;
            set[6] = i;
            set[7] = i;
            set+=8;
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

	if (CCBPtr != &CCBArraySky[0]) drawCCBarrayAny(--CCBPtr, Source, CCBArraySky);
}

static void SegLoop(viswall_t *segl)
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
    if (ActionBits & AC_ADDSKY) {
        skyOn = true;
        if (skyType==SKY_DEFAULT) {
            SegLoopSky(segl, CenterY);
        }
    }
}

/**********************************

	Draw all the sprites from back to front.

**********************************/

static void DrawSprites(void)
{
	Word i;
	Word *LocalPtr;
	vissprite_t *VisPtr;

	i = SpriteTotal;	/* Init the count */
	if (i) {		/* Any sprites to speak of? */
		LocalPtr = SortedSprites;	/* Get the pointer to the sorted array */
		VisPtr = vissprites;	/* Cache pointer to sprite array */
		do {
			DrawVisSprite(&VisPtr[*LocalPtr++&0x7F]);	/* Draw from back to front */
		} while (--i);
	}
}

/**********************************

	Follow the list of walls and draw each
	and every wall fragment.
	Note : I draw the walls closest to farthest and I maintain a ZBuffet

**********************************/

void SegCommands(void)
{
    const bool lightQuality = (depthShadingOption > 1);
	{
        Word i;		/* Temp index */
        viswall_t *WallSegPtr;		/* Pointer to the current wall */
        viswall_t *LastSegPtr;


        WallSegPtr = viswalls;		/* Get the first wall segment to process */
        LastSegPtr = lastwallcmd;	/* Last one to process */
        if (LastSegPtr == WallSegPtr) {	/* No walls to render? */
            return;				/* Exit now!! */
        }


        EnableHardwareClipping();		/* Turn on all hardware clipping to remove slop */

        if (cheatNoclipEnabled) {
            DrawARect(0,0, ScreenWidth, ScreenHeight, 0);   // To avoid HOM when noclipping outside
            FlushCCBs(); // Flush early to render noclip black quad early before everything, for the same reason as sky below
        }

        if (skyOn && (skyType!=SKY_DEFAULT)) {
            drawNewSky(skyType);
            FlushCCBs(); // Flush early to render the sky early before everything, as we hacked the wall renderer to draw earlier than the final flush.
        }
        skyOn = false;

        i = 0;		// Init the vertical clipping records
        do {
            clipboundtop[i] = -1;		// Allow to the ceiling
            clipboundbottom[i] = ScreenHeight;	// Stop at the floor
        } while (++i<ScreenWidth);

        // Process all the wall segments

        scaleArrayData = scaleArray;
        scaleArrayIndex = 0;
        do {
            scaleArrayPtr[scaleArrayIndex++] = scaleArrayData;
            SegLoop(WallSegPtr);
        } while (++WallSegPtr<LastSegPtr);	// Next wall in chain

        // Now I actually draw the walls back to front to allow for clipping because of slop

        LastSegPtr = viswalls;		// Stop at the first one
        do {
            --WallSegPtr;			// Last go backwards!!
            scaleArrayData = scaleArrayPtr[--scaleArrayIndex];

            if (wallQuality == WALL_QUALITY_HI) {
                if (lightQuality) {
                    DrawSegFull(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegFullUnshaded(WallSegPtr, scaleArrayData);
                }
            } else if (wallQuality == WALL_QUALITY_MED) {
                if (lightQuality) {
                    DrawSegHalf(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegHalfUnshaded(WallSegPtr, scaleArrayData);
                }
            } else {
                DrawSegFullFlat(WallSegPtr, scaleArrayData);
            }
        } while (WallSegPtr!=LastSegPtr);
    }

	// Now we draw all the planes. They are already clipped and create no slop!
    {
        visplane_t *PlanePtr;
        visplane_t *LastPlanePtr;
        Word WallScale;

        PlanePtr = visplanes+1;		// Get the range of pointers
        LastPlanePtr = lastvisplane;

        if (PlanePtr!=LastPlanePtr) {	// No planes generated?
            planey = -viewy;		// Get the Y coord for camera
            WallScale = (viewangle-ANG90)>>ANGLETOFINESHIFT;	// left to right mapping
            basexscale = (finecosine[WallScale] / ((int)ScreenWidth/2));
            baseyscale = -(finesine[WallScale] / ((int)ScreenWidth/2));
            do {
                DrawVisPlane(PlanePtr);		// Convert the plane
            } while (++PlanePtr<LastPlanePtr);		// Loop for all
        }
        resetSpanPointer();
    }

	DisableHardwareClipping();		// Sprites require full screen management
	DrawSprites();					// Draw all the sprites (ZSorted and clipped)
}
