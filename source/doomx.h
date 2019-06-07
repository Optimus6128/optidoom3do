// ========= My externs ========

#include <Burger.h>
#include <celutils.h>

#define LIGHTSCALESHIFT 3


// New Sky Types
enum {
    SKY_DEFAULT,
    SKY_GRADIENT_DAY,
    SKY_GRADIENT_NIGHT,
    SKY_GRADIENT_DUSK,
    SKY_GRADIENT_DAWN,
    SKY_PLAYSTATION
};

typedef struct MyCCB {		/* Clone of the CCB Block from the 3DO includes */
	uint32 ccb_Flags;
	struct MyCCB *ccb_NextPtr;
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
} MyCCB;			/* I DON'T include width and height */

typedef struct {
    Word xStart;
    Word xEnd;
	Byte *data;			/* Pointer to raw texture data */
	Word width;			/* Width of texture in pixels */
	Word height;		/* Height of texture in pixels */
	int topheight;		/* Top texture height in global pixels */
	int bottomheight;	/* Bottom texture height in global pixels */
	Word texturemid;	/* Anchor point for texture */
} drawtex_t;

typedef struct {
    int scale;
    Word column;
    Word light;
} viscol_t;


// In phase6.c
extern Word columnWidth;
extern Word wallQuality;
extern Word skyType;
extern Word tx_texturecolumn;
extern Word depthShadingOption;
extern Word thingsShadingOption;
extern Word rendererOption;

extern void initCCBarrayWall(void);
extern void initCCBarrayWallFlat(void);
extern void initCCBarraySky(void);

// In phase6ll.c
extern void DrawSegFullFlatUnshadedLL(viswall_t *segl, int *scaleData);


// In phase7.c
extern Word floorQuality;
extern Word waterfxEnabled;

extern void initCCBarrayFloor(void);
extern void initCCBarrayFloorFlat(void);
extern void initCCBarrayFloorFlatVertical(void);
extern void initSpanDrawFunc(void);


// In threedo.c
extern Word fpsDisplayed;
extern int frameTime;

// In threedor.c
extern Byte *SpanPtr;
extern Byte *CelLine190;            // strange pointer to something having to do with one of the sprite routines
extern Word LightTable[32];

extern void initCCBarray(void);
extern void FlushCCBs(void);
extern void resetSpanPointer(void);
extern void AddCelToCurrentCCB(CCB* cel);
extern void setColorGradient16(int c0, int c1, int r0, int g0, int b0, int r1, int g1, int b1, uint16* bmp);
extern void DrawASpan(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);
extern void DrawASpanLo(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);
extern void clearSpanArray(void);
extern int getSkyScale(unsigned int i);

// In bench.c
extern int getTicks(void);

// In xskies.c
extern Word fireSkyHeight;

extern void initNewSkies(void);
extern int getSkyHeight(unsigned int i);
extern void drawNewSky(int which);
extern void updateFireSkyHeightPal(void);

// In ammain.c

extern Boolean ShowAllLines;
extern Boolean ShowAllThings;
extern Word thickLinesEnabled;
extern Word cheatIDDQDenabled;
extern Word cheatNoclipEnabled;
extern Word flyIsOn;

extern void setAutomapLines(bool enabled);
extern void setAutomapItems(bool enabled);
extern void toggleNoclip(player_t *player);
extern void toggleIDDQD(player_t *player);
extern void applyIDKFA(player_t *player);
extern void toggleFlyMode(player_t *player);

// In tick.c
extern Word playerSpeedOption;
extern Word enemiesSpeedOption;

// In mobj.c
extern Word extraBloodOption;

extern void P_SpawnBloodParticle(Fixed x,Fixed y,Fixed z,Word damage);
extern void P_SpawnPuffParticle(Fixed x,Fixed y,Fixed z);
