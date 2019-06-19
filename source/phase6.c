#include "Doom.h"
#include <IntMath.h>

/**********************************

	This code will draw all the VERTICAL walls for
	a screen.

**********************************/

static viswall_t *WallSegPtr;		// Pointer to the current wall
static viswall_t *LastSegPtr;      // Pointer to last wall

static int scaleArray[MAXSCREENWIDTH * 16];     // MAXSCREENWIDTH * 16 should be more than enough
static int *scaleArrayPtr[MAXWALLCMDS];         // start pointers in scaleArray for each individual line segment
static int scaleArrayIndex;                     // index for new line segment
int *scaleArrayData;                            // pointer to pass scale wall column data to phase6_1


/***************************

	Wall quality settings

****************************/

Word wallQuality = WALL_QUALITY_HI;         // 0=LO(Untextured), 1=MED, 2=HI
Word columnWidth;                           // column width in pixels (1 for fullRes, 2 for halfRes)

Word depthShadingOption = 2;     // 0=OFF(DARK), 1=OFF(BRIGHT), 2=ON
Word thingsShadingOption = 0;    // 0=OFF, 1=ON
Word rendererOption = 0;         // 0=DOOM, 1=LIGOLEAST, 2=LIGOMOST
Word extraRenderOption = 0;      // 0=OFF, 1=WIREFRAME, 2=CYBER

Word skyType = SKY_DEFAULT;



/**********************************

	Follow the list of walls and draw each
	and every wall fragment.
	Note : I draw the walls closest to farthest and I maintain a ZBuffet

**********************************/

static bool SegCommands_Init()
{
    WallSegPtr = viswalls;              // Get the first wall segment to process
    LastSegPtr = lastwallcmd;           // Last one to process
    if (LastSegPtr == WallSegPtr) {     // No walls to render?
        return false;                   // Exit now!!
    }
    return true;
}

static void DrawBackground()
{
    if (cheatNoclipEnabled | extraRenderOption == 1) {
        DrawARect(0,0, ScreenWidth, ScreenHeight, 0);   // To avoid HOM when noclipping outside
        FlushCCBs(); // Flush early to render noclip black quad early before everything, for the same reason as sky below
    }

    if (skyOnView && (skyType!=SKY_DEFAULT)) {
        drawNewSky(skyType);
        FlushCCBs(); // Flush early to render the sky early before everything, as we hacked the wall renderer to draw earlier than the final flush.
    }
    skyOnView = false;
}

static void StartSegLoop()
{
    // Process all the wall segments
    PrepareSegLoop();

    scaleArrayData = scaleArray;
    scaleArrayIndex = 0;
    do {
        scaleArrayPtr[scaleArrayIndex++] = scaleArrayData;
        SegLoop(WallSegPtr);
    } while (++WallSegPtr<LastSegPtr);	// Next wall in chain
}

static void DrawWalls()
{
    // Now I actually draw the walls back to front to allow for clipping because of slop

    const bool lightQuality = (depthShadingOption > 1);

    LastSegPtr = viswalls;		// Stop at the first one

    if (rendererOption==0 && extraRenderOption != 1) {
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
                if (lightQuality) {
                    DrawSegFullFlat(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegFullFlatUnshaded(WallSegPtr, scaleArrayData);
                }
            }
        } while (WallSegPtr!=LastSegPtr);
    } else {

        if (extraRenderOption == 1) {
            DisableHardwareClipping();
        }

        do {
            --WallSegPtr;			// Last go backwards!!
            scaleArrayData = scaleArrayPtr[--scaleArrayIndex];

            DrawSegFullFlatUnshadedLL(WallSegPtr, scaleArrayData);
        } while (WallSegPtr!=LastSegPtr);

        if (extraRenderOption == 1) {
            FlushCCBs();
            EnableHardwareClipping();
        }
    }
}

static void DrawPlanes()
{
	// Now we draw all the planes. They are already clipped and create no slop!

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

static void DrawSprites()
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

void SegCommands()
{
    if (!SegCommands_Init()) return;

    EnableHardwareClipping();		// Turn on all hardware clipping to remove slop

        DrawBackground();

        StartSegLoop();

        DrawWalls();

        if (extraRenderOption != 1) DrawPlanes();

	DisableHardwareClipping();		// Sprites require full screen management

        DrawSprites();		// Draw all the sprites (ZSorted and clipped)
}
