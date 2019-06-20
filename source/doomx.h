// ========= My externs ========

#include <Burger.h>
#include <celutils.h>

#include "doomopts.h"


#define LIGHTSCALESHIFT 3


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
extern int *scaleArrayData;


// In phase6_1.c
extern void SegLoop(viswall_t *segl);
extern void PrepareSegLoop(void);
extern void initCCBarraySky(void);

extern bool skyOnView;


// In phase6_2.c

extern void initCCBarrayWall(void);
extern void initCCBarrayWallFlat(void);

extern void DrawSegFull(viswall_t *segl, int *scaleData);
extern void DrawSegFullUnshaded(viswall_t *segl, int *scaleData);
extern void DrawSegFullFlat(viswall_t *segl, int *scaleData);
extern void DrawSegFullFlatUnshaded(viswall_t *segl, int *scaleData);
extern void DrawSegHalf(viswall_t *segl, int *scaleData);
extern void DrawSegHalfUnshaded(viswall_t *segl, int *scaleData);
extern void drawCCBarray(MyCCB* lastCCB, MyCCB *CCBArrayPtr);  // extern needed for phase6_1.c using this to draw sky columns


// In phase6ll.c
extern void DrawSegFullFlatUnshadedLL(viswall_t *segl, int *scaleData);
extern void initCCBQuadWallFlat(void);


// In phase7.c
extern void initCCBarrayFloor(void);
extern void initCCBarrayFloorFlat(void);
extern void initCCBarrayFloorFlatVertical(void);
extern void initSpanDrawFunc(void);


// In threedo.c
extern int frameTime;
extern void printDbg(int value);

// In threedor.c
extern Byte *SpanPtr;
extern Byte *CelLine190;            // strange pointer to something having to do with one of the sprite routines
extern Word LightTable[32];

extern void initAllCCBelements(void);
extern void FlushCCBs(void);
extern void resetSpanPointer(void);
extern void AddCelToCurrentCCB(CCB* cel);
extern void setColorGradient16(int c0, int c1, int r0, int g0, int b0, int r1, int g1, int b1, uint16* bmp);
extern void DrawASpan(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);
extern void DrawASpanLo(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);
extern void DrawThickLine(Word x1,Word y1,Word x2,Word y2,Word color);
extern void clearSpanArray(void);
extern int getSkyScale(unsigned int i);

// In bench.c
extern int getTicks(void);

// In xskies.c
extern void initNewSkies(void);
extern int getSkyHeight(unsigned int i);
extern void drawNewSky(int which);
extern void updateFireSkyHeightPal(void);

// In ammain.c
extern Boolean ShowAllLines;
extern Boolean ShowAllThings;

extern void setAutomapLines(bool enabled);
extern void setAutomapItems(bool enabled);
extern void toggleNoclip(player_t *player);
extern void toggleIDDQD(player_t *player);
extern void applyIDKFA(player_t *player);
extern void toggleFlyMode(player_t *player);

// In mobj.c
extern void P_SpawnBloodParticle(Fixed x,Fixed y,Fixed z,Word damage);
extern void P_SpawnPuffParticle(Fixed x,Fixed y,Fixed z);

// In omain.c
extern void resetMenuOptions(void);
