// ========= Menu options externs ========

// Graphics presets
enum
{
	PRESET_GFX_MIN,
	PRESET_GFX_ATARI,
	PRESET_GFX_AMIGA,
	PRESET_GFX_SNES,
	PRESET_GFX_GBA,
	PRESET_GFX_JAGUAR,
	PRESET_GFX_DEFAULT,
	PRESET_GFX_FASTER,
	PRESET_GFX_CUSTOM,
	PRESET_GFX_MAX,
	PRESET_OPTIONS_NUM
};

// Stats options
enum
{
	STATS_OFF,
	STATS_FPS,
	STATS_MEM,
	STATS_ALL,
	STATS_OPTIONS_NUM
};

// Wall quality options
enum
{
    WALL_QUALITY_LO,
    WALL_QUALITY_MED,
    WALL_QUALITY_HI,
    WALL_QUALITY_OPTIONS_NUM
};

// Floor quality options
enum
{
    FLOOR_QUALITY_LO,
    FLOOR_QUALITY_MED,
    FLOOR_QUALITY_HI,
    FLOOR_QUALITY_OPTIONS_NUM
};

// Screen scaling options
enum
{
    SCREEN_SCALE_1x1,
    SCREEN_SCALE_2x1,
    SCREEN_SCALE_1x2,
    SCREEN_SCALE_2x2,
    SCREEN_SCALE_OPTIONS_NUM
};

// Depth shading options
enum
{
    DEPTH_SHADING_DARK,
    DEPTH_SHADING_BRIGHT,
    DEPTH_SHADING_ON,
    DEPTH_SHADING_OPTIONS_NUM
};

// Renderer options
enum
{
    RENDERER_DOOM,
    RENDERER_POLY,
    RENDERER_OPTIONS_NUM
};

// Frame limiter options
enum
{
	FRAME_LIMIT_OFF,
	FRAME_LIMIT_15,
	FRAME_LIMIT_20,
	FRAME_LIMIT_30,
	FRAME_LIMIT_60,
	FRAME_LIMIT_OPTIONS_NUM
};

// New Sky Types
enum {
    SKY_DEFAULT,
    SKY_GRADIENT_DAY,
    SKY_GRADIENT_NIGHT,
    SKY_GRADIENT_DUSK,
    SKY_GRADIENT_DAWN,
    SKY_PLAYSTATION,
    SKY_OPTIONS_NUM
};

// Extra render options
enum
{
    GIMMICKS_OFF,
    GIMMICKS_WIREFRAME,
    GIMMICKS_CUBE,
    GIMMICKS_DISTORT,
    GIMMICKS_MOTION_BLUR,
    GIMMICKS_OPTIONS_NUM
};

// Cheats revealed options
enum
{
    CHEATS_OFF,
    CHEATS_HALF,    // Only automap and noclip
    CHEATS_FULL,    // IDDQD and IDKFA visible
    CHEATS_REVEALED_OPTIONS_NUM
};

// Automap cheat options
enum
{
    AUTOMAP_CHEAT_OFF,
    AUTOMAP_CHEAT_SHOWTHINGS,
    AUTOMAP_CHEAT_SHOWLINES,
    AUTOMAP_CHEAT_SHOWALL,
    AUTOMAP_OPTIONS_NUM
};

// Player speed options
enum
{
    PLAYER_SPEED_1X,
    PLAYER_SPEED_1_5X,
    PLAYER_SPEED_2X,
    PLAYER_SPEED_OPTIONS_NUM
};

// Enemy speed options
enum
{
    ENEMY_SPEED_0X,
    ENEMY_SPEED_0_5X,
    ENEMY_SPEED_1X,
    ENEMY_SPEED_2X,
    ENEMY_SPEED_OPTIONS_NUM
};

typedef struct GraphicsOptions
{
	Word frameLimit;
	Word screenSizeIndex;
	Word wallQuality;
	Word floorQuality;
	Word screenScale;
	Word fitToScreen;
	Word depthShading;
	Word thingsShading;
	Word renderer;
}GraphicsOptions;

typedef struct OtherOptions
{
	Word stats;
	Word gimmicks;
	Word thickLines;
	Word waterFx;
	Word sky;
	Word fireSkyHeight;
	Word cheatsRevealed;
    Word cheatAutomap;
    Word cheatIDKFAdummy;
	Word cheatNoclip;
	Word cheatIDDQD;
	Word playerSpeed;
	Word enemySpeed;
	Word extraBlood;
	Word fly;
}OtherOptions;

typedef struct AllOptions
{
	GraphicsOptions graphics;
	OtherOptions other;
}AllOptions;

// In omain.c
extern Word presets;
extern GraphicsOptions *optGraphics;
extern OtherOptions *optOther;

extern Word opt_dbg1;
extern Word opt_dbg2;
extern Word opt_dbg3;
extern Word opt_dbg4;
extern Word opt_dbg5;
extern Word opt_dbg6;
extern Word opt_dbg7;
extern Word opt_dbg8;
