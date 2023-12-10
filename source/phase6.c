#include "Doom.h"
#include <IntMath.h>

/**********************************

	This code will draw all the VERTICAL walls for
	a screen.

**********************************/

#define MID_DIST 384
#define FAR_DIST 768


static viswall_t *WallSegPtr;		// Pointer to the current wall
static viswall_t *LastSegPtr;      // Pointer to last wall

static ColumnStore columnStoreArray[MAXSCREENWIDTH * 16];	// MAXSCREENWIDTH * 16 should be more than enough
static ColumnStore *columnStoreArrayPtr[MAXWALLCMDS];		// start pointers in columnStoreArray for each individual line segment
static int columnStoreArrayIndex;							// index for new line segment
ColumnStore *columnStoreArrayData;							// pointer to pass scale and light wall column data to phase6_1


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

static void prepHeuristicSegInfo()
{
	const Fixed playerX = players.mo->x;
	const Fixed playerY = players.mo->y;
	viswall_t *viswall = WallSegPtr;
    do {
		seg_t *seg = viswall->SegPtr;
		vertex_t *v1 = &seg->v1;
		vertex_t *v2 = &seg->v2;
		const int wallCenterX = (v1->x + v2->x) >> 1;
		const int wallCenterY = (v1->y + v2->y) >> 1;
		const int wallDist = GetApproxDistance(wallCenterX - playerX, wallCenterY - playerY) >> 16;

		if (wallDist > FAR_DIST) {
			viswall->renderKind = VW_FAR;
		} else if (wallDist > MID_DIST) {
			viswall->renderKind = VW_MID;
		} else {
			viswall->renderKind = VW_CLOSE;
		}
    } while (++viswall!=LastSegPtr);
}

static void StartSegLoop()
{
    // Process all the wall segments
    PrepareSegLoop();

	prepHeuristicSegInfo();

    columnStoreArrayData = columnStoreArray;
    columnStoreArrayIndex = 0;
    do {
		// Commenting also out the idea that didn't work, but could work in the future
		//if (!isSegWallOccluded(WallSegPtr)) {
			columnStoreArrayPtr[columnStoreArrayIndex++] = columnStoreArrayData;
			SegLoop(WallSegPtr);
		/*} else {
			columnStoreArrayPtr[columnStoreArrayIndex++] = NULL;
		}*/
    } while (++WallSegPtr!=LastSegPtr);	// Next wall in chain
}

static void DrawWalls()
{
    // Now I actually draw the walls back to front to allow for clipping because of slop

    LastSegPtr = viswalls;		// Stop at the first one
    if (optGraphics->renderer == RENDERER_DOOM) {
        do {
            --WallSegPtr;			// Last go backwards!!
			columnStoreArrayData = columnStoreArrayPtr[--columnStoreArrayIndex];
			if (columnStoreArrayData) {
				if (optGraphics->wallQuality == WALL_QUALITY_HI) {
					DrawSeg(WallSegPtr, columnStoreArrayData);
				} else {
					DrawSegFlat(WallSegPtr, columnStoreArrayData);
				}
			}
        } while (WallSegPtr!=LastSegPtr);
    } else {
		bool renderSwitchColumns = false;
		bool prevRenderSwitchColumns = false;
        do {
            --WallSegPtr;			// Last go backwards!!
			columnStoreArrayData = columnStoreArrayPtr[--columnStoreArrayIndex];
			if (columnStoreArrayData) {
				if (optGraphics->wallQuality ==  WALL_QUALITY_LO) {  // flat
					DrawSegPoly(WallSegPtr, columnStoreArrayData); // so, always poly
				} else {
					if (WallSegPtr->renderKind >= VW_FAR) {
						renderSwitchColumns = true;
					} else {
						renderSwitchColumns = false;
					}

					if (renderSwitchColumns) {
						if (renderSwitchColumns != prevRenderSwitchColumns) {
							flushCCBarrayPolyWall();
						}
						DrawSeg(WallSegPtr, columnStoreArrayData);
					} else {
						if (renderSwitchColumns != prevRenderSwitchColumns) {
							flushCCBarrayWall();
						}
						DrawSegPoly(WallSegPtr, columnStoreArrayData);  // go poly anyway
					}
					prevRenderSwitchColumns = renderSwitchColumns;
				}
			}
        } while (WallSegPtr!=LastSegPtr);

		flushCCBarrayPolyWall();
    }
	flushCCBarrayWall();
}

void DrawWallsWireframe()
{
    LastSegPtr = viswalls;

    DisableHardwareClippingWithoutFlush();

    do {
		--WallSegPtr;
        columnStoreArrayData = columnStoreArrayPtr[--columnStoreArrayIndex];
		if (columnStoreArrayData) {
			DrawSegWireframe(WallSegPtr, columnStoreArrayData);
		}
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

	//startBenchPeriod(3, "DrawSprites");
		DrawSprites();		// Draw all the sprites (ZSorted and clipped)
	//endBenchPeriod(3);
}

void SegCommands()
{
    if (!SegCommands_Init()) return;

		EnableHardwareClipping();		// Turn on all hardware clipping to remove slop

        DrawBackground();

//startBenchPeriod(0, "StartSegLoop");
        StartSegLoop();
//endBenchPeriod(0);

        if (enableWireframeMode) {
            DrawWallsWireframe();
        } else {

//startBenchPeriod(1, "DrawWalls");
            DrawWalls();
//endBenchPeriod(1);

//startBenchPeriod(2, "DrawPlanes");
            DrawPlanes();
//endBenchPeriod(2);
        }
}
