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


bool background_clear = false;


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
    if (background_clear | optOther->cheatNoclip | enableWireframeMode) {
        DrawARect(0,0, ScreenWidth, ScreenHeight, 0);   // To avoid HOM when noclipping outside
        FlushCCBs(); // Flush early to render noclip black quad early before everything, for the same reason as sky below
    }

    if (skyOnView && (optOther->sky!=SKY_DEFAULT) && !enableWireframeMode) {
        drawNewSky(optOther->sky);
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

    const bool lightShadingOn = (optGraphics->depthShading >= DEPTH_SHADING_DITHERED);
    bool tooTightForPoly = false;

    LastSegPtr = viswalls;		// Stop at the first one
    if (optGraphics->renderer == RENDERER_DOOM) {
        do {
            --WallSegPtr;			// Last go backwards!!
            scaleArrayData = scaleArrayPtr[--scaleArrayIndex];

            WallSegPtr->distance >>= VISWALL_DISTANCE_PRESHIFT;
            if (optGraphics->wallQuality == WALL_QUALITY_HI) {
                if (lightShadingOn) {
                    DrawSeg(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegUnshaded(WallSegPtr, scaleArrayData);
                }
            } else {
                if (lightShadingOn) {
                    DrawSegFlat(WallSegPtr, scaleArrayData);
                } else {
                    DrawSegFlatUnshaded(WallSegPtr, scaleArrayData);
                }
            }
        } while (WallSegPtr!=LastSegPtr);
    } else {
    	bool previousWallsTooTight = false;
        do {
            --WallSegPtr;			// Last go backwards!!
            scaleArrayData = scaleArrayPtr[--scaleArrayIndex];

            if (optGraphics->wallQuality ==  WALL_QUALITY_LO) {  // flat
                DrawSegPoly(WallSegPtr, scaleArrayData); // so, always poly
            } else {
                const int pixLength = WallSegPtr->RightX - WallSegPtr->LeftX + 1;
                const int threshold = 8;

                if (pixLength < threshold) {
                    tooTightForPoly = true;
                } else {
                	const int texWidth = WallSegPtr->t_texture->width;  // using top texture (center and top) for now
                	int texLength;

					texLeft = (WallSegPtr->offset - IMFixMul( finetangent[(WallSegPtr->CenterAngle+xtoviewangle[WallSegPtr->LeftX])>>ANGLETOFINESHIFT], WallSegPtr->distance)) >> FRACBITS;
					texRight = (WallSegPtr->offset - IMFixMul( finetangent[(WallSegPtr->CenterAngle+xtoviewangle[WallSegPtr->RightX])>>ANGLETOFINESHIFT], WallSegPtr->distance)) >> FRACBITS;
                	texLength = texRight - texLeft;

                    if (texLength < 1) texLength = 1;
                    tooTightForPoly = ((pixLength * texWidth) / texLength) < threshold;
                }

                WallSegPtr->distance >>= VISWALL_DISTANCE_PRESHIFT;
                if (tooTightForPoly) {
					if (lightShadingOn) {
						DrawSeg(WallSegPtr, scaleArrayData);
					} else {
						DrawSegUnshaded(WallSegPtr, scaleArrayData);
					}
					previousWallsTooTight = true;
                } else {
                	if (previousWallsTooTight) {
						flushCCBarrayWall();
						previousWallsTooTight = false;
                	}
                    DrawSegPoly(WallSegPtr, scaleArrayData);  // go poly anyway
                }
            }

        } while (WallSegPtr!=LastSegPtr);
    }
	flushCCBarrayWall();
}

void DrawWallsWireframe()
{
    LastSegPtr = viswalls;

    DisableHardwareClippingWithoutFlush();

    do {
        scaleArrayData = scaleArrayPtr[--scaleArrayIndex];
        DrawSegWireframe(--WallSegPtr, scaleArrayData);
    } while (WallSegPtr!=LastSegPtr);

    FlushCCBs();
    EnableHardwareClipping();
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
        flushCCBarrayPlane();
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

void SegCommandsSprites()
{
	DisableHardwareClipping();		// Sprites require full screen management

	DrawSprites();		// Draw all the sprites (ZSorted and clipped)
}

void SegCommands()
{
    if (!SegCommands_Init()) return;

		EnableHardwareClipping();		// Turn on all hardware clipping to remove slop

        DrawBackground();

        StartSegLoop();

        if (enableWireframeMode) {
            DrawWallsWireframe();
        } else {
            DrawWalls();
            DrawPlanes();
        }
}
