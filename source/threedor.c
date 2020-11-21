#include "Doom.h"
#include <Portfolio.h>
#include <event.h>
#include <Init3do.h>

#include "stdio.h"
#include <IntMath.h>
#include <celutils.h>

/**********************************

	This contains all the 3DO specific calls for Doom

**********************************/




#define CCBTotal 0x100

static MyCCB CCBArray[CCBTotal];		/* Array of CCB structs */
static MyCCB *CurrentCCB = &CCBArray[0];	/* Pointer to empty CCB */

Byte *CelLine190;

Byte SpanArray[MAXSCREENWIDTH*MAXSCREENHEIGHT];	/* Buffer for plane textures */
Byte *SpanPtr = SpanArray;		/* Pointer to empty buffer */


// New n/16 dark shades LightTable that doesn't require USEAV on the CEL
// We scrap USEAV because we can the use the 5 AV bits in the future for fog instead (shade to white)

Word LightTable[32] = {
 0x0301,0x0701,0x0B01,0x0F01,0x1301,0x1701,0x1B01,0x1F01,
 0x03C1,0x03C1,0x07C1,0x07C1,0x0BC1,0x0BC1,0x0FC1,0x0FC1,
 0x13C1,0x13C1,0x17C1,0x17C1,0x1BC1,0x1BC1,0x1FC1,0x1FC1,
 0x1FC1,0x1FC1,0x1FC1,0x1FC1,0x1FC1,0x1FC1,0x1FC1,0x1FC1
};

/*Word LightTableFog[32] = {
 0x1F7E,0x1F7C,0x1F7A,0x1F78,0x1F76,0x1F74,0x1F72,0x1F70,
 0x1F6E,0x1F6C,0x1F6A,0x1F68,0x1F66,0x1F64,0x1F62,0x1F60,
 0x1F5E,0x1F5C,0x1F5A,0x1F58,0x1F56,0x1F54,0x1F52,0x1F50,
 0x1F4E,0x1F4C,0x1F4A,0x1F48,0x1F46,0x1F44,0x1F42,0x1F40
};*/

static void initCCBarray(void)
{
	MyCCB *CCBPtr;
	int i = CCBTotal;

	CCBPtr = CCBArray;
	do {
		CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);	/* Create the next offset */
		CCBPtr->ccb_HDDX = 0;	/* Set the defaults */
		CCBPtr->ccb_HDDY = 0;
		++CCBPtr;
	} while (--i);
}

void initAllCCBelements()
{
    initCCBarray();
    initWallCELs();
    initCCBarraySky();
    initPlaneCELs();
    initCCBQuadWallFlat();
    initCCBQuadWallTextured();

	if (enableNewSkies) initNewSkies();
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

void resetSpanPointer()
{
	SpanPtr = SpanArray;		/* Reset the plane texture pointer */
}

void FlushCCBs(void)
{
	MyCCB* NewCCB;

	NewCCB = CurrentCCB;
	if (NewCCB!=&CCBArray[0]) {
		--NewCCB;		/* Get the last used CCB */
		NewCCB->ccb_Flags |= CCB_LAST;	/* Mark as the last one */
		DrawCels(VideoItem,(CCB *)&CCBArray[0]);	/* Draw all the cels in one shot */
		CurrentCCB = &CCBArray[0];		/* Reset the empty entry */
	}
    resetSpanPointer();
}

void AddCelToCurrentCCB(CCB* cel)
{
	MyCCB* DestCCB;			// Pointer to new CCB entry
	LongWord TheFlags;		// CCB flags
	LongWord ThePtr;		// Temp pointer to munge

	DestCCB = CurrentCCB;		// Copy pointer to local
	if (DestCCB>=&CCBArray[CCBTotal]) {		// Am I full already?
		FlushCCBs();
		DestCCB = CCBArray;
	}
	TheFlags = cel->ccb_Flags;		// Preload the CCB flags
	DestCCB->ccb_XPos = cel->ccb_XPos;		// Set the x and y coord
	DestCCB->ccb_YPos = cel->ccb_YPos;
	DestCCB->ccb_HDX = cel->ccb_HDX;	// Set the data for the CCB
	DestCCB->ccb_HDY = cel->ccb_HDY;
	DestCCB->ccb_VDX = cel->ccb_VDX;
	DestCCB->ccb_VDY = cel->ccb_VDY;
	DestCCB->ccb_PIXC = cel->ccb_PIXC;
	DestCCB->ccb_PRE0 = cel->ccb_PRE0;
	DestCCB->ccb_PRE1 = cel->ccb_PRE1;

	ThePtr = (LongWord)cel->ccb_SourcePtr;	// Force absolute address
	DestCCB->ccb_SourcePtr = (CelData *)ThePtr;	// Save the source ptr
	DestCCB->ccb_PLUTPtr = cel->ccb_PLUTPtr;

	DestCCB->ccb_Flags = (TheFlags & ~(CCB_LAST|CCB_NPABS)) | (CCB_SPABS|CCB_PPABS);    // Not sure what this does, but without it it fails

	++DestCCB;			// Next CCB
	CurrentCCB = DestCCB;	// Save the CCB pointer
}

void setColorGradient16(int c0, int c1, int r0, int g0, int b0, int r1, int g1, int b1, uint16* bmp)
{
    int i, rr, gg, bb;
    float dc = (float)(c1 - c0);
    float dr = (float)(r1 - r0) / dc;
    float dg = (float)(g1 - g0) / dc;
    float db = (float)(b1 - b0) / dc;
    float r = (float)r0;
    float g = (float)g0;
    float b = (float)b0;

    bmp+=c0;
    for (i=c0; i<=c1; i++)
    {
        rr = (int)r >> 3;
        gg = (int)g >> 3;
        bb = (int)b >> 3;
        *bmp++ = (rr << 10) | (gg << 5) | bb;
        r += dr;
        g += dg;
        b += db;
    }
}

/**********************************

	Draw a scaled solid line

**********************************/

static void AddCCB(Word x,Word y,MyCCB* NewCCB)
{
	MyCCB* DestCCB;			/* Pointer to new CCB entry */
	LongWord TheFlags;		/* CCB flags */
	LongWord ThePtr;		/* Temp pointer to munge */

	DestCCB = CurrentCCB;		/* Copy pointer to local */
	if (DestCCB>=&CCBArray[CCBTotal]) {		/* Am I full already? */
		FlushCCBs();
		DestCCB = CCBArray;
	}
	TheFlags = NewCCB->ccb_Flags;		/* Preload the CCB flags */
	DestCCB->ccb_XPos = x<<16;		/* Set the x and y coord */
	DestCCB->ccb_YPos = y<<16;
	DestCCB->ccb_HDX = NewCCB->ccb_HDX;	/* Set the data for the CCB */
	DestCCB->ccb_HDY = NewCCB->ccb_HDY;
	DestCCB->ccb_VDX = NewCCB->ccb_VDX;
	DestCCB->ccb_VDY = NewCCB->ccb_VDY;
	DestCCB->ccb_PIXC = NewCCB->ccb_PIXC;
	DestCCB->ccb_PRE0 = NewCCB->ccb_PRE0;
	DestCCB->ccb_PRE1 = NewCCB->ccb_PRE1;

	ThePtr = (LongWord)NewCCB->ccb_SourcePtr;	/* Force absolute address */
	if (!(TheFlags&CCB_SPABS)) {
		ThePtr += ((LongWord)NewCCB)+12;	/* Convert relative to abs */
	}
	DestCCB->ccb_SourcePtr = (CelData *)ThePtr;	/* Save the source ptr */

	if (TheFlags&CCB_LDPLUT) {		/* Only load a new plut if REALLY needed */
		ThePtr = (LongWord)NewCCB->ccb_PLUTPtr;
		if (!(TheFlags&CCB_PPABS)) {		/* Relative plut? */
			ThePtr += ((LongWord)NewCCB)+16;	/* Convert relative to abs */
		}
		DestCCB->ccb_PLUTPtr = (void *)ThePtr;	/* Save the PLUT pointer */
	}
	DestCCB->ccb_Flags = (TheFlags & ~(CCB_LAST|CCB_NPABS)) | (CCB_SPABS|CCB_PPABS);
	++DestCCB;			/* Next CCB */
	CurrentCCB = DestCCB;	/* Save the CCB pointer */
}

/**********************************

	Draw a masked shape on the screen

**********************************/

void DrawMShape(Word x,Word y,void *ShapePtr)
{
	((MyCCB*)ShapePtr)->ccb_Flags &= ~CCB_BGND;	/* Enable masking */
	AddCCB(x,y,(MyCCB*)ShapePtr);		/* Draw the shape */
}

/**********************************

	Draw an unmasked shape on the screen

**********************************/

void DrawShape(Word x,Word y,void *ShapePtr)
{
	((MyCCB*)ShapePtr)->ccb_Flags |= CCB_BGND;	/* Disable masking */
	AddCCB(x,y,(MyCCB*)ShapePtr);		/* Draw the shape */
}

/**********************************

	Draw a solid colored line using the 3DO
	cel engine hardware. This replaces the generic code
	found in AMMain.c.
	My brain hurts.

**********************************/

void DrawLine(Word x1,Word y1,Word x2,Word y2,Word color,Word thickness)
{
	MyCCB* DestCCB;			/* Pointer to new CCB entry */
	int thickOffset = thickness >> 1;

	DestCCB = CurrentCCB;		/* Copy pointer to local */
	if (DestCCB>=&CCBArray[CCBTotal]) {		/* Am I full already? */
		FlushCCBs();				/* Draw all the CCBs/Lines */
		DestCCB=CCBArray;
	}
	DestCCB->ccb_Flags = CCB_LDSIZE|CCB_LDPRS|
		CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
		CCB_ACE|CCB_BGND|CCB_NOBLK;	/* ccb_flags */

	DestCCB->ccb_PIXC = 0x1F00;		/* PIXC control */
	DestCCB->ccb_PRE0 = 0x40000016;		/* Preamble */
	DestCCB->ccb_PRE1 = 0x03FF1000;		/* Second preamble */
	DestCCB->ccb_SourcePtr = (CelData *)0;	/* Save the source ptr */
	DestCCB->ccb_PLUTPtr = (void *)(color<<16);		/* Set the color pixel */

	if ((int)x2<(int)x1) {	/* By sorting the x and y's I can only draw lines */
		Word Temp;		/* in two types, x>=y or x<y */
		Temp = x1;		/* Swap the x's */
		x1 = x2;
		x2 = Temp;
		Temp = y1;		/* Swap the y's */
		y1 = y2;
		y2 = Temp;
	}

	y1=-y1;			/* The y's are upside down!! */
	y2=-y2;

	x2=(x2-x1)+1;	/* Get the DELTA value (Always positive) */
	y2=y2-y1;		/* But add 1 for inclusive size depending on sign */
	x1+=160;	/* Move the x coords to the CENTER of the screen */
	y1+=80;		/* Vertically flip and then CENTER the y */

	if (y2&0x8000) {	/* Negative y? */
		y2-=1;			/* Widen by 1 pixel */
		if (x2<(-y2)) {	/* Quadrant 7? */
			x1-=1;
			y1+=1;
			goto Quadrant7;	/* OK */
		}		/* Quadrant 6 */
		goto Quadrant6;		/* OK */
	}
	++y2;
	if (x2<y2) {	/* Quadrant 7? */
Quadrant7:
		DestCCB->ccb_HDX = thickness<<20;
		DestCCB->ccb_HDY = 0<<20;
		DestCCB->ccb_VDX = x2<<16;
		DestCCB->ccb_VDY = y2<<16;
		x1 -= thickOffset;
	} else {		/* Quadrant 6 */
		--y1;
Quadrant6:
		DestCCB->ccb_VDX = 0<<16;
		DestCCB->ccb_VDY = thickness<<16;
		DestCCB->ccb_HDX = x2<<20;
		DestCCB->ccb_HDY = y2<<20;
		y1 -= thickOffset;
	}

	DestCCB->ccb_XPos = x1<<16;		/* Set the x and y coord for start */
	DestCCB->ccb_YPos = y1<<16;

	++DestCCB;			/* Next CCB */
	CurrentCCB = DestCCB;	/* Save the CCB pointer */
}

void DrawThickLine(Word x1,Word y1,Word x2,Word y2,Word color)
{
    if (optOther->thickLines) {
        const Word darkColor = ((((color >> 10) & 31) >> 1) << 10) | ((((color >> 5) & 31) >> 1) << 5) | ((color & 31) >> 1);
        DrawLine(x1,y1,x2,y2,darkColor,3);
    }
    DrawLine(x1,y1,x2,y2,color,1);
}


/**********************************

	This code is functionally equivalent to the Burgerlib
	version except that it is using the cached CCB system.

**********************************/

void DrawARect(Word x1,Word y1,Word Width,Word Height,Word color)
{
	MyCCB* DestCCB;			/* Pointer to new CCB entry */

	if (Width==0 || Height==0) return;

	DestCCB = CurrentCCB;		/* Copy pointer to local */
	if (DestCCB>=&CCBArray[CCBTotal]) {		/* Am I full already? */
		FlushCCBs();				/* Draw all the CCBs/Lines */
		DestCCB=CCBArray;
	}
	DestCCB->ccb_Flags = CCB_LDSIZE|CCB_LDPRS|
		CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
		CCB_ACE|CCB_BGND|CCB_NOBLK;	/* ccb_flags */

	DestCCB->ccb_PIXC = 0x1F00;		/* PIXC control */
	DestCCB->ccb_PRE0 = 0x40000016;		/* Preamble */
	DestCCB->ccb_PRE1 = 0x03FF1000;		/* Second preamble */
	DestCCB->ccb_SourcePtr = (CelData *)0;	/* Save the source ptr */
	DestCCB->ccb_PLUTPtr = (void *)(color<<16);		/* Set the color pixel */
	DestCCB->ccb_XPos = x1<<16;		/* Set the x and y coord for start */
	DestCCB->ccb_YPos = y1<<16;
	DestCCB->ccb_HDX = Width<<20;		/* OK */
	DestCCB->ccb_HDY = 0<<20;
	DestCCB->ccb_VDX = 0<<16;
	DestCCB->ccb_VDY = Height<<16;
	++DestCCB;			/* Next CCB */
	CurrentCCB = DestCCB;	/* Save the CCB pointer */
}

/**********************************

	Ok boys amd girls, the follow functions are drawing primitives
	for Doom 3DO that take into account the fact that I draw all lines from
	back to front. This way I use painter's algorithm to do clipping since the
	3DO cel engine does not allow arbitrary clipping of any kind.

	On the first pass, I draw all the sky lines without any regard for clipping
	since no object can be behind the sky.

	Next I then sort the sprites from back to front.

	Then I draw the walls from back to front, mixing in all the sprites so
	that all shapes will clip properly using painter's algorithm.

**********************************/

// OptiDoom comments: I removed the DrawFloor and DrawWall functions and others from here, as I handle them in a different manner and I cleaned up these. However I keep the comments here because they are important.

/**********************************

	Drawing the floor and ceiling is the hardest, so I'll do this last.
	The parms are,
	x = screen x coord for the virtual screen. Must be offset
		to the true screen coords.
	top = screen y coord for the virtual screen. Must also be offset
		to the true screen coords.
	bottom = screen y coord for the BOTTOM of the pixel run. Subtract from top
		to get the exact destination pixel run count.
	colnum = index for which scan line to draw from the source image. Note that
		this number has all bits set so I must mask off the unneeded bits for
		texture wraparound.

	SpanPtr is a pointer to the SpanArray buffer, this is where I store the
	processed floor textures.
	No light shading is used. The scale factor is a constant.

**********************************/

void clearSpanArray()
{
    memset(SpanArray, 0, MAXSCREENWIDTH*MAXSCREENHEIGHT);
}


/**********************************

	This routine will draw a scaled sprite during the game.
	It is called when there is no onscreen clipping needed or
	if the only clipping is to the screen bounds.

**********************************/

void DrawSpriteNoClip(vissprite_t *vis)
{
	patch_t	*patch;		/* Pointer to the actual sprite record */
	Word ColorMap;
	int x;

	patch = (patch_t *)LoadAResource(vis->PatchLump);
	patch =(patch_t *) &((Byte *)patch)[vis->PatchOffset];

	((LongWord *)patch)[7] = 0;
	((LongWord *)patch)[10] = 0;
	((LongWord *)patch)[8] = vis->yscale<<4;
	ColorMap = vis->colormap;
	if (ColorMap&0x8000) {
		((LongWord *)patch)[13] = 0x9C81;
	} else {
		((LongWord *)patch)[13] = LightTable[((ColorMap&0xFF) * spriteLight) >>(8 + LIGHTSCALESHIFT)];
	}
	if (ColorMap&0x4000) {
		x = vis->x2;
		((LongWord *)patch)[9] = -vis->xscale;
	} else {
		x = vis->x1;
		((LongWord *)patch)[9] = vis->xscale;
	}
	DrawMShape(x+ScreenXOffset,vis->y1+ScreenYOffset,&patch->Data);
	ReleaseAResource(vis->PatchLump);
}

/**********************************

	This routine will draw a scaled sprite during the game.
	It is called when there is onscreen clipping needed so I
	use the global table spropening to get the top and bottom clip
	bounds.

	I am passed the screen clipped x1 and x2 coords.

**********************************/

static Byte *SpritePLUT;
static Word SpriteY;
static Word SpriteYScale;
static Word SpritePIXC;
static Word SpritePRE0;
static Word SpritePRE1;
static Byte *StartLinePtr;
static Word SpriteWidth;

static void OneSpriteLine(Word x1,Byte *SpriteLinePtr)
{
	MyCCB *DestCCB;

	DestCCB = CurrentCCB;		/* Copy pointer to local */
	if (DestCCB>=&CCBArray[CCBTotal]) {		/* Am I full already? */
		FlushCCBs();				/* Draw all the CCBs/Lines */
		DestCCB=CCBArray;
	}
	DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_PACKED|
	CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
	CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT;	/* ccb_flags */

	DestCCB->ccb_PIXC = SpritePIXC;			/* PIXC control */
	DestCCB->ccb_PRE0 = SpritePRE0;		/* Preamble (Coded 8 bit) */
	DestCCB->ccb_PRE1 = SpritePRE1;		/* Second preamble */
	DestCCB->ccb_SourcePtr = (CelData *)SpriteLinePtr;	/* Save the source ptr */
	DestCCB->ccb_PLUTPtr = SpritePLUT;		/* Get the palette ptr */
	DestCCB->ccb_XPos = (x1+ScreenXOffset)<<16;		/* Set the x and y coord for start */
	DestCCB->ccb_YPos = SpriteY;
	DestCCB->ccb_HDX = 0<<20;		/* OK */
	DestCCB->ccb_HDY = SpriteYScale;
	DestCCB->ccb_VDX = 1<<16;
	DestCCB->ccb_VDY = 0<<16;
	++DestCCB;			/* Next CCB */
	CurrentCCB = DestCCB;	/* Save the CCB pointer */
}

static void OneSpriteClipLine(Word x1,Byte *SpriteLinePtr,int Clip,int Run)
{
	MyCCB *DestCCB;

	DrawARect(0,191,Run,1,BLACK);
	DestCCB = CurrentCCB;		/* Copy pointer to local */
	if (DestCCB>=&CCBArray[CCBTotal-1]) {		/* Am I full already? */
		FlushCCBs();				/* Draw all the CCBs/Lines */
		DestCCB=CCBArray;
	}
	DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_PACKED|
	CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
	CCB_ACE|CCB_BGND|CCB_PPABS|CCB_LDPLUT;	/* ccb_flags */

	DestCCB->ccb_PIXC = 0x1F00;			/* PIXC control */
	DestCCB->ccb_PRE0 = SpritePRE0;		/* Preamble (Coded 8 bit) */
	DestCCB->ccb_PRE1 = SpritePRE1;		/* Second preamble */
	DestCCB->ccb_SourcePtr = (CelData *)SpriteLinePtr;	/* Save the source ptr */
	DestCCB->ccb_PLUTPtr = SpritePLUT;		/* Get the palette ptr */
	DestCCB->ccb_XPos = -(Clip<<16);		/* Set the x and y coord for start */
	DestCCB->ccb_YPos = 191<<16;
	DestCCB->ccb_HDX = SpriteYScale;		/* OK */
	DestCCB->ccb_HDY = 0<<20;
	DestCCB->ccb_VDX = 0<<16;
	DestCCB->ccb_VDY = 1<<16;
	++DestCCB;			/* Next CCB */

	DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
	CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
	CCB_ACE|CCB_NOBLK|CCB_PPABS;	/* ccb_flags */

	DestCCB->ccb_PIXC = SpritePIXC;			/* PIXC control */
	DestCCB->ccb_PRE0 = 0x00000016;		/* Preamble (Uncoded 16) */
	DestCCB->ccb_PRE1 = 0x9E001800+(Run-1);		/* Second preamble */
	DestCCB->ccb_SourcePtr = (CelData *)CelLine190;	/* Save the source ptr */
	DestCCB->ccb_XPos = (x1+ScreenXOffset)<<16;		/* Set the x and y coord for start */
	DestCCB->ccb_YPos = SpriteY+(Clip<<16);
	DestCCB->ccb_HDX = 0<<20;		/* OK */
	DestCCB->ccb_HDY = 1<<20;
	DestCCB->ccb_VDX = 1<<15;		/* Need 15 to fix the LFORM bug */
	DestCCB->ccb_VDY = 0<<16;
	++DestCCB;			/* Next CCB */

	CurrentCCB = DestCCB;	/* Save the CCB pointer */
}

static Byte *CalcLine(Fixed XFrac)
{
	Byte *DataPtr;

	DataPtr = StartLinePtr;
	XFrac>>=FRACBITS;
	if (XFrac<=0) {		/* Left clip failsafe */
		return DataPtr;
	}
	if (XFrac>=SpriteWidth) {	/* Clipping failsafe */
		XFrac=SpriteWidth-1;
	}
	do {
		Word Offset;
		Offset = DataPtr[0]+2;
		DataPtr = &DataPtr[Offset*4];
	} while (--XFrac);
	return DataPtr;
}

void DrawSpriteClip(Word x1,Word x2,vissprite_t *vis)
{
	Word y,MaxRun;
	Word y2;
	Word top,bottom;
	patch_t *patch;
	Fixed XStep,XFrac;

	patch = (patch_t *)LoadAResource(vis->PatchLump);	/* Get shape data */
	patch =(patch_t *) &((Byte *)patch)[vis->PatchOffset];	/* Get true pointer */
	SpriteYScale = vis->yscale<<4;		/* Get scale Y factor */
	SpritePLUT = &((Byte *)patch)[64];	/* Get pointer to PLUT */
	SpritePRE0 = ((Word *)patch)[14]&~(0xFF<<6);	/* Only 1 row allowed! */
	SpritePRE1 = ((Word *)patch)[15];		/* Get the proper height */
	y = ((Word *)patch)[3];		/* Get offset to the sprite shape data */
	StartLinePtr = &((Byte *)patch)[y+16];	/* Get pointer to first line of data */
	SpriteWidth = GetShapeHeight(&((Word *)patch)[1]);
	SpritePIXC = (vis->colormap&0x8000) ? 0x9C81 : LightTable[((vis->colormap&0xFF) * spriteLight) >> (8 + LIGHTSCALESHIFT)];
	y = vis->y1;
	SpriteY = (y+ScreenYOffset)<<16;	/* Unmolested Y coord */
	y2 = vis->y2;
	MaxRun = y2-y;

	if ((int)y<0) {
		y = 0;
	}
	if ((int)y2>=(int)ScreenHeight) {
		y2 = ScreenHeight;
	}
	XFrac = 0;
	XStep = 0xFFFFFFFFUL/(LongWord)vis->xscale;	/* Get the recipocal for the X scale */
	if (vis->colormap&0x4000) {
		XStep = -XStep;		/* Step in the opposite direction */
		XFrac = (SpriteWidth<<FRACBITS)-1;
	}
	if (vis->x1!=x1) {		/* How far should I skip? */
		XFrac += XStep*(x1-vis->x1);
	}
	do {
		top = spropening[x1];		/* Get the opening to the screen */
		if (top==ScreenHeight) {		/* Not clipped? */
			OneSpriteLine(x1,CalcLine(XFrac));
		} else {
			bottom = top&0xff;
			top >>=8;
			if (top<bottom) {		/* Valid run? */
				if (y>=top && y2<bottom) {
					OneSpriteLine(x1,CalcLine(XFrac));
				} else {
					int Run;
					int Clip;

					Clip = top-vis->y1;		/* Number of pixels to clip */
					Run = bottom-top;		/* Allowable run */
					if (Clip<0) {		/* Overrun? */
						Run += Clip;	/* Remove from run */
						Clip = 0;
					}
					if (Run>0) {		/* Still visible? */
						if (Run>MaxRun) {		/* Too big? */
							Run = MaxRun;		/* Force largest... */
						}
						OneSpriteClipLine(x1,CalcLine(XFrac),Clip,Run);
					}
				}
			}
		}
		XFrac+=XStep;
	} while (++x1<=x2);
}

/**********************************

	Draw a sprite in the center of the screen.
	This is used for the finale.
	(Speed is NOT an issue here...)

**********************************/

void DrawSpriteCenter(Word SpriteNum)
{
	Word x,y;
	patch_t *patch;
	LongWord Offset;

	patch = (patch_t *)LoadAResource(SpriteNum>>FF_SPRITESHIFT);	/* Get the sprite group */
	Offset = ((LongWord *)patch)[SpriteNum & FF_FRAMEMASK];
	if (Offset&PT_NOROTATE) {		/* Do I rotate? */
		patch = (patch_t *) &((Byte *)patch)[Offset & 0x3FFFFFFF];		/* Get pointer to rotation list */
		Offset = ((LongWord *)patch)[0];		/* Use the rotated offset */
	}
	patch = (patch_t *)&((Byte *)patch)[Offset & 0x3FFFFFFF];	/* Get pointer to patch */

	x = patch->leftoffset;		/* Get the x and y offsets */
	y = patch->topoffset;
	x = 80-x;			/* Center on the screen */
	y = 90-y;
	((LongWord *)patch)[7] = 0;		/* Compensate for sideways scaling */
	((LongWord *)patch)[10] = 0;
	if (Offset&PT_FLIP) {
		((LongWord *)patch)[8] = -0x2<<20;	/* Reverse horizontal */
		x+=GetShapeHeight(&patch->Data);	/* Adjust the x coord */
	} else {
		((LongWord *)patch)[8] = 0x2<<20;	/* Normal horizontal */
	}
	((LongWord *)patch)[9] = 0x2<<16;		/* Double vertical */
	DrawMShape(x*2,y*2,&patch->Data);		/* Scale the x and y and draw */
	ReleaseAResource(SpriteNum>>FF_SPRITESHIFT);	/* Let go of the resource */
}

/**********************************

	Draw a screen color overlay if needed

**********************************/

void DrawColors(void)
{
	MyCCB* DestCCB;			/* Pointer to new CCB entry */
	player_t *player;
	Word ccb,color;
	Word red,green,blue;

	player = &players;
	if (player->powers[pw_invulnerability] > 240		/* Full strength */
		|| (player->powers[pw_invulnerability]&16) ) {	/* Flashing... */
		color = 0x7FFF<<16;
		ccb = CCB_LDSIZE|CCB_LDPRS|CCB_PXOR|
		CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
		CCB_ACE|CCB_BGND|CCB_NOBLK;
		goto DrawIt;
	}

	red = player->damagecount;		/* Get damage inflicted */
	green = player->bonuscount>>2;
	red += green;
	blue = 0;

	if (player->powers[pw_ironfeet] > 240		/* Radiation suit? */
		|| (player->powers[pw_ironfeet]&16) ) {	/* Flashing... */
		green = 10;			/* Add some green */
	}

	if (player->powers[pw_strength]			/* Berserker pack? */
		&& (player->powers[pw_strength]< 255) ) {
		color = 255-player->powers[pw_strength];
		color >>= 4;
		red+=color;		/* Feeling good! */
	}

	if (red>=32) {
		red = 31;
	}
	if (green>=32) {
		green =31;
	}
	if (blue>=32) {
		blue = 31;
	}

	color = (red<<10)|(green<<5)|blue;

	if (!color) {
		return;
	}
	color <<=16;
	ccb = CCB_LDSIZE|CCB_LDPRS|
		CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
		CCB_ACE|CCB_BGND|CCB_NOBLK;

DrawIt:
	DestCCB = CurrentCCB;		/* Copy pointer to local */
	if (DestCCB>=&CCBArray[CCBTotal]) {		/* Am I full already? */
		FlushCCBs();				/* Draw all the CCBs/Lines */
		DestCCB=CCBArray;
	}

	DestCCB->ccb_Flags =ccb;	/* ccb_flags */
	DestCCB->ccb_PIXC = 0x1F80;		/* PIXC control */
	DestCCB->ccb_PRE0 = 0x40000016;		/* Preamble */
	DestCCB->ccb_PRE1 = 0x03FF1000;		/* Second preamble */
	DestCCB->ccb_SourcePtr = (CelData *)0;	/* Save the source ptr */
	DestCCB->ccb_PLUTPtr = (void *)color;		/* Set the color pixel */
	DestCCB->ccb_XPos = ScreenXOffset<<16;		/* Set the x and y coord for start */
	DestCCB->ccb_YPos = ScreenYOffset<<16;
	DestCCB->ccb_HDX = ScreenWidth<<20;		/* OK */
	DestCCB->ccb_HDY = 0<<20;
	DestCCB->ccb_VDX = 0<<16;
	DestCCB->ccb_VDY = ScreenHeight<<16;
	++DestCCB;			/* Next CCB */
	CurrentCCB = DestCCB;	/* Save the CCB pointer */
}
