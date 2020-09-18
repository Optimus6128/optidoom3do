// ========= Menu options externs ========

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
    SCREEN_SCALE_1x2,
    SCREEN_SCALE_2x1,
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
    GIMMICKS_WARP,
    GIMMICKS_LSD,
    GIMMICKS_CYBER,
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



// In omain.c
extern Word opt_fps;
extern Word opt_wallQuality;
extern Word opt_floorQuality;
extern Word opt_depthShading;
extern Word opt_fitToScreen;
extern Word opt_thingsShading;
extern Word opt_renderer;
extern Word opt_gimmicks;
extern Word opt_thickLines;
extern Word opt_waterfx;
extern Word opt_sky;
extern Word opt_fireSkyHeight;
extern Word opt_cheatNoclip;
extern Word opt_cheatIDDQD;
extern Word opt_playerSpeed;
extern Word opt_enemySpeed;
extern Word opt_extraBlood;
extern Word opt_fly;

extern Word opt_dbg1;
extern Word opt_dbg2;
extern Word opt_dbg3;
extern Word opt_dbg4;
extern Word opt_dbg5;
extern Word opt_dbg6;
extern Word opt_dbg7;
extern Word opt_dbg8;
