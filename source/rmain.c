#include "Doom.h"

#include <celutils.h>

#include "engine_main.h"
#include "engine_grid.h"
#include "engine_mesh.h"
#include "engine_texture.h"
#include "procgen_mesh.h"
#include "bench.h"


/* Data */

viswall_t viswalls[MAXWALLCMDS];		/* Visible wall array */
viswall_t *lastwallcmd;					/* Pointer to free wall entry */
visplane_t visplanes[MAXVISPLANES];		/* Visible floor array */
visplane_t *lastvisplane;				/* Pointer to free floor entry */
vissprite_t	vissprites[MAXVISSPRITES];	/* Visible sprite array */
vissprite_t *vissprite_p;				/* Pointer to free sprite entry */
Byte openings[MAXOPENINGS];
Byte *lastopening;
Fixed viewx,viewy,viewz;	/* Camera x,y,z */
angle_t viewangle;		/* Camera angle */
Fixed viewcos,viewsin;	/* Camera sine,cosine from angle */
Word validcount;		/* Increment every time a check is made */
Word extralight;		/* bumped light from gun blasts */
angle_t clipangle;		/* Leftmost clipping angle */
angle_t doubleclipangle; /* Doubled leftmost clipping angle */


int offscreenPage = SCREENS-1;

static CCB *offscreenCel;
static Mesh *cubeMesh;
static Texture *feedbackTex;

static void updateOffscreenCel(CCB *cel)
{
	const int screenStartOffset = ((ScreenYOffset >> 1) * 320 * 2 + (ScreenYOffset & 1) + (ScreenXOffset << 1)) * 2;
	const int woffset = 320 - 2;
	const int vcnt = (ScreenHeight / 2) - 1;
	Byte *offscreenVramPointer = (Byte*)(getVideoPointer(offscreenPage) + screenStartOffset);

	cel->ccb_SourcePtr = (CelData*)offscreenVramPointer;
	cel->ccb_PRE0 = PRE0_LINEAR | 6 | (vcnt << 6);
	cel->ccb_PRE1 = PRE1_TLLSB_PDC0 | PRE1_LRFORM | (woffset << 16) | (ScreenWidth-1);
	cel->ccb_Width = ScreenWidth;
	cel->ccb_Height = ScreenHeight;

	if (opt_gimmicks == GIMMICKS_MOTION_BLUR) {
		offscreenCel->ccb_PIXC = 0x1F811F81;
	} else {
		offscreenCel->ccb_PIXC = PIXC_OPAQUE;
	}
}

static void renderGimmick3D()
{
	int i;
	const int t = getTicks() >> 4;
	setMeshPosition(cubeMesh, 0, 32, 640);
	setMeshRotation(cubeMesh, t, t >> 1, t >> 2);

	transformGeometry(cubeMesh);

	for (i=0; i<cubeMesh->quadsNum; ++i) {
		QuadData *qd = &cubeMesh->quad[i];
		updateOffscreenCel(qd->cel);
		cubeMesh->tex[qd->textureId].width = ScreenWidth;
		cubeMesh->tex[qd->textureId].height = ScreenHeight;
	}

	renderTransformedGeometry(cubeMesh);
}

void setupOffscreenCel()
{
	updateOffscreenCel(offscreenCel);

	if (opt_fitToScreen) {
		offscreenCel->ccb_XPos = 0;
		offscreenCel->ccb_YPos = 0;
		offscreenCel->ccb_HDX = (320 << 20) / ScreenWidth;
		offscreenCel->ccb_VDY = (160 << 16) / ScreenHeight;
	} else {
		offscreenCel->ccb_XPos = ScreenXOffsetUnscaled << 16;
		offscreenCel->ccb_YPos = ScreenYOffsetUnscaled << 16;
		offscreenCel->ccb_HDX = (1 + screenScaleX) << 20;
		offscreenCel->ccb_VDY = (1 + screenScaleY) << 16;
	}

	updateScreenGridCels();
}

static void renderOffscreenBufferGrid()
{
	const int t = getTicks() >> 6;

	switch(opt_gimmicks) {
		case GIMMICKS_DISTORT:
			updateGridFx(GRID_FX_DISTORT, t);
		break;

		case GIMMICKS_WARP:
			updateGridFx(GRID_FX_WARP, t);
		break;

		default:
		break;
	}

	renderGrid();
}

static void renderOffscreenBuffer()
{
	AddCelToCurrentCCB(offscreenCel);
	FlushCCBs();
}

/**********************************

	Init the math tables for the refresh system

**********************************/

void R_Init(void)
{
	R_InitData();			/* Init the data (Via loading or calculations) */
	clipangle = xtoviewangle[0];	/* Get the left clip angle from viewport */
	doubleclipangle = clipangle*2;	/* Precalc angle * 2 */

	offscreenCel = CreateCel(1, 1, 16, CREATECEL_UNCODED, getVideoPointer(offscreenPage));
	offscreenCel->ccb_Flags |= CCB_BGND;

	feedbackTex = initFeedbackTexture(0, 0, 320, 200, SCREENS);
	cubeMesh = initGenMesh(256, feedbackTex, MESH_OPTION_CPU_CCW_TEST, MESH_CUBE, NULL);
	setMeshPolygonOrder(cubeMesh, true, true);

	initGrid(8, 8);
}

/**********************************

	Setup variables for a 3D refresh based on
	the current camera location and angle

**********************************/

void R_Setup(void)
{
	player_t *player;			/* Local player pointer */
	mobj_t *mo;					/* Object record of the player */
	Word i;

/* set up globals for new frame */

	player = &players;	/* Which player is the camera attached? */
	mo = player->mo;
	viewx = mo->x;					/* Get the position of the player */
	viewy = mo->y;
	viewz = player->viewz;			/* Get the height of the player */
	viewangle = mo->angle;	/* Get the angle of the player */

	i = viewangle>>ANGLETOFINESHIFT;	/* Precalc angle index */
	viewsin = finesine[i];		/* Get the base sine value */
	viewcos = finecosine[i];		/* Get the base cosine value */

	extralight = player->extralight << 6;	/* Init the extra lighting value */

	lastvisplane = visplanes+1;		/* visplanes[0] is left empty */
	lastwallcmd = viswalls;			/* No walls added yet */
	vissprite_p = vissprites;		/* No sprites added yet */
	lastopening = openings;			/* No openings found */
}

/**********************************

	Render the 3D view

**********************************/

void R_RenderPlayerView (void)
{
	R_Setup();		/* Init variables based on camera angle */
	BSP();			/* Traverse the BSP tree for possible walls to render */

	FlushCCBs();
	if (useOffscreenBuffer || useOffscreenGrid) {
		SetMyScreen(offscreenPage);	// Offscreen buffer is the last
	}

	SegCommands();	/* Draw all everything Z Sorted */
	DrawColors();	/* Draw color overlay if needed */

	FlushCCBs();
    if (useOffscreenBuffer || useOffscreenGrid) {
		SetMyScreen(WorkPage);	// Must restore visible screenpage if previously set to offscreen
		if (useOffscreenGrid) {
			renderOffscreenBufferGrid();
		} else {
			renderOffscreenBuffer();
			if (opt_gimmicks == GIMMICKS_CUBE) {
				renderGimmick3D();
			}
		}
    }

    DrawWeapons();		/* Draw the weapons on top of the screen */
    FlushCCBs();
}
