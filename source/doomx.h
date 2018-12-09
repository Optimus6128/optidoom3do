// ========= My externs ========

#include <Burger.h>
#include <celutils.h>

// New Sky Types
enum {
    SKY_DEFAULT,
    SKY_GRADIENT_DAY,
    SKY_GRADIENT_NIGHT,
    SKY_GRADIENT_DUSK,
    SKY_GRADIENT_DAWN,
    SKY_PLAYSTATION
};

// In phase6.c
extern Word columnWidth;
extern Word wallQuality;
extern Word skyType;

// In phase7.c
extern Word floorQuality;
extern Word waterfxEnabled;

// In threedo.c
extern Word fpsDisplayed;
extern int frameTime;

// In threedor.c
extern Byte *CelLine190;            // strange pointer to something having to do with one of the sprite routines

extern void initCCBarray(void);
extern void FlushCCBs(void);
extern void AddCelToCurrentCCB(CCB* cel);
extern void setColorGradient16(int c0, int c1, int r0, int g0, int b0, int r1, int g1, int b1, uint16* bmp);
extern void DrawASpan(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,Byte *Dest);
extern void clearSpanArray(void);
extern int getSkyScale(unsigned int i);

// In ammain.c
extern Word thickLinesEnabled;

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
