// ========= Menu options externs ========

// New Sky Types
enum {
    SKY_DEFAULT,
    SKY_GRADIENT_DAY,
    SKY_GRADIENT_NIGHT,
    SKY_GRADIENT_DUSK,
    SKY_GRADIENT_DAWN,
    SKY_PLAYSTATION
};

// Wall quality options
enum
{
    WALL_QUALITY_LO = 0,
    WALL_QUALITY_MED = 1,
    WALL_QUALITY_HI = 2
};

// Floor quality options
enum
{
    FLOOR_QUALITY_LO = 0,
    FLOOR_QUALITY_MED = 1,
    FLOOR_QUALITY_HI = 2
};

// Depth shading options
enum
{
    DEPTH_SHADING_DARK = 0,
    DEPTH_SHADING_BRIGHT = 1,
    DEPTH_SHADING_ON = 2
};

// Renderer options
enum
{
    RENDERER_DOOM = 0,
    RENDERER_LIGOLEAST = 1
};

// Extra render options
enum
{
    EXTRA_RENDER_OFF = 0,
    EXTRA_RENDER_WIREFRAME = 1,
    EXTRA_RENDER_CYBER = 2
};

// Cheats revealed options
enum
{
    CHEATS_OFF = 0,
    CHEATS_HALF = 1,    // Only automap and noclip
    CHEATS_FULL = 2     // IDDQD and IDKFA visible
};

// Automap cheat options
enum
{
    AUTOMAP_CHEAT_OFF = 0,
    AUTOMAP_CHEAT_SHOWTHINGS = 1,
    AUTOMAP_CHEAT_SHOWLINES = 2
};

// Player speed options
enum
{
    PLAYER_SPEED_1X = 0,
    PLAYER_SPEED_1_5X = 1,
    PLAYER_SPEED_2X = 2
};

// Enemy speed options
enum
{
    ENEMY_SPEED_0X = 0,
    ENEMY_SPEED_0_5X = 1,
    ENEMY_SPEED_1X = 2,
    ENEMY_SPEED_2X = 3
};



// In omain.c
extern Word opt_fps;
extern Word opt_wallQuality;
extern Word opt_floorQuality;
extern Word opt_depthShading;
extern Word opt_thingsShading;
extern Word opt_renderer;
extern Word opt_extraRender;
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
