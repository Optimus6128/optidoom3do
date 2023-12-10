// ========= My externs ========

#include <Burger.h>
#include <celutils.h>

#include "doomopts.h"


#define LIGHTSCALESHIFT 3
#define SEC_SPEC_RENDER_BITS 0xFFA0
#define SEC_SPEC_ORIG_BITS 0x1F

#define CLAMP(value,min,max) if ((value)<(min)) (value)=(min); if ((value)>(max)) (value)=(max);
#define CLAMP_LEFT(value,min) if ((value)<(min)) (value)=(min);
#define CLAMP_RIGHT(value,max) if ((value)>(max)) (value)=(max);

// Minimal CCB that excludes HDDX,HDDY,CCB_Width and CCB_Height. Used for 2d scaled bitmaps and columns.
typedef struct BitmapCCB {
	uint32 ccb_Flags;
	struct BitmapCCB *ccb_NextPtr;
	CelData    *ccb_SourcePtr;
	void       *ccb_PLUTPtr;
	Coord ccb_XPos;
	Coord ccb_YPos;
	int32  ccb_HDX;
	int32  ccb_HDY;
	int32  ccb_VDX;
	int32  ccb_VDY;
	uint32 ccb_PIXC;
	uint32 ccb_PRE0;
	uint32 ccb_PRE1;
} BitmapCCB;

// Minimal CCB that excludes CCB_Width and CCB_Height but still needs HDDX and HDDY. Used for polygon walls mode.
typedef struct PolyCCB {
	uint32 ccb_Flags;
	struct PolyCCB *ccb_NextPtr;
	CelData    *ccb_SourcePtr;
	void       *ccb_PLUTPtr;
	Coord ccb_XPos;
	Coord ccb_YPos;
	int32  ccb_HDX;
	int32  ccb_HDY;
	int32  ccb_VDX;
	int32  ccb_VDY;
	int32  ccb_HDDX;
	int32  ccb_HDDY;
	uint32 ccb_PIXC;
	uint32 ccb_PRE0;
	uint32 ccb_PRE1;
} PolyCCB;

typedef PolyCCB MyCCB;	// The old MyCCB still needed to refer to loaded resources, is the same as the PolyCCB

typedef struct {
    Word xStart;
    Word xEnd;
	Byte *data;			/* Pointer to raw texture data */
	Word width;			/* Width of texture in pixels */
	Word height;		/* Height of texture in pixels */
	int topheight;		/* Top texture height in global pixels */
	int bottomheight;	/* Bottom texture height in global pixels */
	Word texturemid;	/* Anchor point for texture */
	Word color;			/* color for flat untextured shading */
} drawtex_t;

typedef struct {
    int scale;
    Word column;
    Word light;
} viscol_t;

typedef struct {
	int scale;
	int light;
} ColumnStore;

typedef enum {
	SEC_SPEC_FOG = 32,
	SEC_SPEC_DISTORT = 64,
	SEC_SPEC_SCROLL = 512,
	SEC_SPEC_DIRS = 384,
	SEC_SPEC_SCROLL_OR_WARP = 896
} viswallspecial_e;

typedef enum {
	LOADING_FIX_OFF,
	LOADING_FIX_ON,
	LOADING_FIX_RELAXED
} loading_fix_t;


// In phase6.c
extern ColumnStore *columnStoreArrayData;
extern bool background_clear;


// In phase6_1.c
void initCCBarraySky(void);
void SegLoop(viswall_t *segl);
void PrepareSegLoop(void);
bool isSegWallOccluded(viswall_t *segl);

extern bool skyOnView;


// In phase6_2.c
void initWallCELs(void);
void DrawSeg(viswall_t *segl, ColumnStore *columnStoreData);
void DrawSegFlat(viswall_t *segl, ColumnStore *columnStoreData);
void flushCCBarrayWall(void);

extern uint32* CCBflagsAlteredIndexPtr[MAXWALLCMDS];	// Array of pointers to CEL flags to set/remove LD_PLUT

// In phase6PL.c
void DrawSegPoly(viswall_t *segl, ColumnStore *columnStoreData);
void DrawSegWireframe(viswall_t *segl, ColumnStore *columnStoreData);
void initCCBQuadWallFlat(void);
void initCCBQuadWallTextured(void);
void flushCCBarrayPolyWall(void);


// In phase7.c
void initPlaneCELs(void);
void flushCCBarrayPlane(void);

// In phase8.c
extern Word spriteLight;

// In wad_loader.c
extern int loadingFix;

// In rdata.c
extern int screenScaleX;
extern int screenScaleY;
void initScreenSizeValues(void);

// In rmain.c
extern int offscreenPage;
extern bool enableWireframeMode;
void setupOffscreenCel(void);

// In threedo.c
extern int frameTime;
extern int displayFreq;
extern Word WorkPage;
void SetMyScreen(Word Page);
Byte *getVideoPointer(Word Page);
LongWord getVBLtic(void);
void drawLoadingBar(int pos, int max, const char *text);
void drawDebugValue(int value);
void printDbg(int value);
void printDbgMinMax(int value);
void updateScreenAndWait(void);
void DisableHardwareClippingWithoutFlush(void);
void updateGamma(int value, int max, bool shouldUpdateVDL);
void updateVDL(bool invert);
void colorizeVDL(int r, int g, int b);
void copyMainScreenToRest(void);

// In threedor.c
extern Byte *SpanPtr;
extern Byte *CelLine190;            // strange pointer to something having to do with one of the sprite routines
extern Word LightTable[32];
extern Word LightTableFog[32];
extern bool testEnableFog;
extern CCB *dummyCCB;				// this is to reset HDDX and HDDY to zero

void initAllCCBelements(void);
void initDummyCCB(void);
void drawCCBarray(BitmapCCB *lastCCB, BitmapCCB *CCBArrayPtr);
void FlushCCBs(void);
void resetSpanPointer(void);
void AddCelToCurrentCCB(CCB* cel);
void setColorGradient16(int c0, int c1, int r0, int g0, int b0, int r1, int g1, int b1, uint16* bmp);
void DrawASpan(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);
void DrawASpanLo(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);
void DrawThickLine(Word x1,Word y1,Word x2,Word y2,Word color);
void clearSpanArray(void);
void initColoredPals(uint16 *srcPal, uint16 *dstPal, int numCols, Word colorMul);

// In xskies.c
void initFireSky(void);
void deinitFireSky(void);
void drawNewSky(int which);
int getSkyScale(void);
void updateFireSkyHeightPal(void);

// In ammain.c
extern Boolean ShowAllLines;
extern Boolean ShowAllThings;

void setAutomapLines(bool enabled);
void setAutomapItems(bool enabled);
void toggleNoclip(player_t *player);
void toggleIDDQD(player_t *player);
void applyIDKFA(player_t *player);
void toggleFlyMode(player_t *player);

// In mobj.c
void P_SpawnBloodParticle(Fixed x,Fixed y,Fixed z,Word damage);
void P_SpawnPuffParticle(Fixed x,Fixed y,Fixed z);

// In omain.c
extern bool useOffscreenBuffer;
void resetMenuOptions(void);
void setPrimaryMenuOptions(void);
void setScreenSizeSliderFromOption(void);


// In modmenu.c
extern char *wadSelected;
extern bool debugMode;
extern bool loadPsxSamples;
extern bool enableFireSky;
extern bool enableWaterFx;
extern bool enableSectorColors;
void startModMenu(void);
void drawText(int xtp, int ytp, char *text);
void drawNumber(int xtp, int ytp, int number);

// In setup.c
extern uint16 flatTextureColors[MAX_UNIQUE_TEXTURES];
