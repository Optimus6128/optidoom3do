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

Word columnWidth;                           // column width in pixels (1 for fullRes, 2 for halfRes)

bool background_clear = false;
bool specialWireframeCase;


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
    if (background_clear | opt_cheatNoclip | specialWireframeCase) {
        DrawARect(0,0, ScreenWidth, ScreenHeight, 0);   // To avoid HOM when noclipping outside
        FlushCCBs(); // Flush early to render noclip black quad early before everything, for the same reason as sky below
    }

    if (skyOnView && (opt_sky!=SKY_DEFAULT) && !specialWireframeCase) {
        drawNewSky(opt_sky);
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

    const bool lightShadingOn = (opt_depthShading == DEPTH_SHADING_ON);

    LastSegPtr = viswalls;		// Stop at the first one

    fullWallPartCount = 0;
    brokenWallPartCount = 0;

    if (opt_renderer == RENDERER_DOOM && !specialWireframeCase) {
        do {
            --WallSegPtr;			// Last go backwards!!
            scaleArrayData = scaleArrayPtr[--scaleArrayIndex];

            if (opt_wallQuality == WALL_QUALITY_HI) {
                if (lightShadingOn) {
                    DrawSegFull(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegFullUnshaded(WallSegPtr, scaleArrayData);
                }
            } else if (opt_wallQuality == WALL_QUALITY_MED) {
                if (lightShadingOn) {
                    DrawSegHalf(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegHalfUnshaded(WallSegPtr, scaleArrayData);
                }
            } else {
                if (lightShadingOn) {
                    DrawSegFullFlat(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegFullFlatUnshaded(WallSegPtr, scaleArrayData);
                }
            }
        } while (WallSegPtr!=LastSegPtr);
    } else {

        do {
            --WallSegPtr;			// Last go backwards!!
            scaleArrayData = scaleArrayPtr[--scaleArrayIndex];

            DrawSegUnshadedLL(WallSegPtr, scaleArrayData);

        } while (WallSegPtr!=LastSegPtr);

        if (opt_extraRender == EXTRA_RENDER_WIREFRAME) {
            DisableHardwareClippingWithoutFlush();
            FlushCCBs();
            EnableHardwareClipping();
        }

        printDbg(fullWallPartCount);
        printDbg(brokenWallPartCount);
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

    specialWireframeCase = (opt_extraRender == EXTRA_RENDER_WIREFRAME && opt_wallQuality == WALL_QUALITY_LO);

    EnableHardwareClipping();		// Turn on all hardware clipping to remove slop

        DrawBackground();

        StartSegLoop();

        DrawWalls();

        if (!specialWireframeCase) DrawPlanes();

	DisableHardwareClipping();		// Sprites require full screen management

        DrawSprites();		// Draw all the sprites (ZSorted and clipped)
}
