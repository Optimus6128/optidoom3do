#include "Doom.h"
#include <String.h>

//#define DEBUG_MENU_HACK

#define OPTION_OFFSET_X     16      // Horizontal pixel offset of option name from end of label
#define SLIDER_POSX         106     // X coord for slider bars
#define SLIDER_WIDTH        88      // Slider width

// ======== Enums and variables for Skull and Slider ========

static Word cursorFrame;		// Skull animation frame
static Word cursorCount;		// Time mark to animate the skull
static Word cursorPos;			// Y position of the skull
static Word moveCount;			// Time mark to move the skull
static Word toggleIDKFAcount;        // A count to turn IDKFA exclamation mark off after triggered (because it doesn't have two option states)

enum { BAR, HANDLE};    // Sub shapes for slider bar

static void *sliderShapes;


// ======== Enums and variables for specialized Control Options ========
    // Action buttons can be set to PadA, PadB, or PadC

    enum {SFU,SUF,FSU,FUS,USF,UFS, NUM_CONTROL_OPTIONS};

    static char SpeedTxt[] = "Speed";		// Local ASCII
    static char FireTxt[] = "Fire";
    static char UseTxt[] = "Use";
    static char *buttona[NUM_CONTROL_OPTIONS] =
            {SpeedTxt,SpeedTxt,FireTxt,FireTxt,UseTxt,UseTxt};
    static char *buttonb[NUM_CONTROL_OPTIONS] =
            {FireTxt,UseTxt,SpeedTxt,UseTxt,SpeedTxt,FireTxt};
    static char *buttonc[NUM_CONTROL_OPTIONS] =
            {UseTxt,FireTxt,UseTxt,SpeedTxt,FireTxt,SpeedTxt};

    static Word configuration[NUM_CONTROL_OPTIONS][3] = {
        {PadA,PadB,PadC},
        {PadA,PadC,PadB},
        {PadB,PadA,PadC},
        {PadC,PadA,PadB},
        {PadB,PadC,PadA},
        {PadC,PadB,PadA}
    };


// ======== Variables for all the option values ========

    Word opt_dbg1;
    Word opt_dbg2;
    Word opt_dbg3;
    Word opt_dbg4;

    Word opt_fps;
    Word opt_wallQuality;
    Word opt_floorQuality;
    Word opt_depthShading;
    Word opt_thingsShading;
    Word opt_renderer;
    Word opt_extraRender;
    Word opt_thickLines;
    Word opt_waterFx;
    Word opt_sky;
    Word opt_fireSkyHeight;
    Word opt_cheatNoclip;
    Word opt_cheatIDDQD;
    Word opt_playerSpeed;
    Word opt_enemySpeed;
    Word opt_extraBlood;
    Word opt_fly;

    static Word opt_screenSizeIndex;
    static Word opt_cheatsRevealed = 0;
    static Word opt_cheatAutomap = 0;
    static Word opt_cheatIDKFAdummy = 0;

    static bool isMusicStopped = false;


// =============================
//   Menu items to select from
// =============================

enum {
	mi_soundVolume,     // Sfx Volume
	mi_musicVolume,     // Music volume
	mi_controls,        // Control settings

#ifdef DEBUG_MENU_HACK
    mi_dbg1,
    mi_dbg2,
    mi_dbg3,
    mi_dbg4,
#endif

	mi_fps,             // FPS display on/off
	mi_screenSize,      // Screen size settings
	mi_wallQuality,     // Wall quality (fullres(hi), halfres(med), untextured(lo))
	mi_floorQuality,    // Floor/Ceiling quality (textured, flat)
	mi_shading_depth,   // Depth shading option (on, off (dark/bright))
	mi_shading_items,   // Shading enable option for items (weapons, enemies, things, etc)
	mi_renderer,        // Selection of the new renderers (polygons instead of columns, etc)
	mi_extra_render,    // Extra experimental rendering things (pure wireframe, etc)
	mi_mapLines,        // Map thick lines on/off
	mi_waterFx,         // Water fx on/off
	mi_sky,             // New skies
	mi_firesky_slider,  // Slider to change firesky height
	mi_enableCheats,    // Switch to enable cheats
	mi_cheatAutomap,    // Automap enable cheat
	mi_cheatNoclip,     // Clip through walls cheat
	mi_cheatIDDQD,      // Invulnerability cheat
	mi_cheatIDKFA,      // All keys and ammo cheat
	mi_playerSpeed,     // Player speed (1x, 2x)
	mi_enemySpeed,      // Enemy speed (0x, 1x, 2x)
	mi_extraBlood,      // Extra blood particles
	mi_flyMode,         // Fly mode
	NUM_MENUITEMS
};

enum {
    page_audio,
    page_controls,
#ifdef DEBUG_MENU_HACK
    page_debug,
#endif
    page_performance,
    page_rendering,
    page_effects,
    page_cheats,
    page_extra,
    NUM_PAGES
};

#define AUDIOSLIDERS_OPTIONS_NUM 16
#define CONTROLS_OPTIONS_NUM 6
#define CHEATSREVEALED_OPTIONS_NUM 3

#define DEBUG1_OPTIONS_NUM 4
#define DEBUG2_OPTIONS_NUM 2
#define DEBUG3_OPTIONS_NUM 2
#define DEBUG4_OPTIONS_NUM 3

#define WALLQUALITY_OPTIONS_NUM 3
#define FLOORQUALITY_OPTIONS_NUM 3
#define DEPTHSHADING_OPTIONS_NUM 3
#define RENDERER_OPTIONS_NUM 2
#define EXTRA_RENDER_OPTIONS_NUM 2
#define OFFON_OPTIONS_NUM 2
#define AUTOMAP_OPTIONS_NUM 4
#define DUMMYIDKFA_OPTIONS_NUM 2
#define THICKLINES_OPTIONS_NUM 2
#define SKY_OPTIONS_NUM 6
#define SKY_HEIGHTS_OPTIONS_NUM 4
#define PLAYER_SPEED_OPTIONS_NUM 3
#define ENEMY_SPEED_OPTIONS_NUM 4


static char *offOnOptions[OFFON_OPTIONS_NUM] = { "OFF", "ON" };
static char *wallQualityOptions[WALLQUALITY_OPTIONS_NUM] = { "LO", "MED", "HI"};
static char *floorQualityOptions[FLOORQUALITY_OPTIONS_NUM] = { "LO", "MED", "HI" };
static char *depthShadingOptions[DEPTHSHADING_OPTIONS_NUM] = { "DARK", "BRIGHT", "ON" };
static char *rendererOptions[RENDERER_OPTIONS_NUM] = { "DOOM", "POLY" };
static char *extraRenderOptions[EXTRA_RENDER_OPTIONS_NUM] = { "OFF", "WIREFRAME" };
static char *automapOptions[AUTOMAP_OPTIONS_NUM] = { "OFF", "THINGS", "LINES", "ALL" };
static char *dummyIDKFAoptions[DUMMYIDKFA_OPTIONS_NUM] = { " ", "!" };
static char *thicklinesOptions[THICKLINES_OPTIONS_NUM] = { "NORMAL", "THICK" };
static char *skyOptions[SKY_OPTIONS_NUM] = { "DEFAULT", "DAY", "NIGHT", "DUSK", "DAWN", "PSX" };
static char *playerSpeedOptions[PLAYER_SPEED_OPTIONS_NUM] = { "1X", "1.5X", "2X" };
static char *enemySpeedOptions[ENEMY_SPEED_OPTIONS_NUM] = { "0X", "0.5X", "1X", "2X" };


enum {
    muiStyle_special = 0,
    muiStyle_text = 1,
    muiStyle_slider = 2
};

typedef struct {
    int posX, posY;
    char *label;
    bool centered;
    Word muiStyle;
    Word *optionValuePtr;
    Word optionsRange;
    char **optionName;
    bool changed;
    bool loops;
    bool show;
}MenuItem;


static MenuItem menuItems[NUM_MENUITEMS];
static Word itemPage[NUM_MENUITEMS];

static char *pageLabel[NUM_PAGES] = { "AUDIO", "CONTROLS",
#ifdef DEBUG_MENU_HACK
"DEBUG",
#endif
"PERFORMANCE", "RENDERING", "EFFECTS", "CHEATS", "EXTRA" };


static void setMenuItem(Word id, int posX, int posY, char *label, bool centered, Word muiStyle, Word *optionValuePtr, Word optionsRange)
{
    menuItems[id].posX = posX;
    menuItems[id].posY = posY;
    menuItems[id].label = label;
    menuItems[id].centered = centered;
    menuItems[id].muiStyle = muiStyle;
    menuItems[id].optionValuePtr = optionValuePtr;
    menuItems[id].optionsRange = optionsRange;

    menuItems[id].changed = false;
    menuItems[id].loops = !(muiStyle & muiStyle_slider);
    menuItems[id].show = true;

    menuItems[id].optionName = 0;   // null for special cases where we don't provide slider nor option names array (e.g. control options, specialized drawing from old code but nothing except label from us)
}

static void setMenuItemOptionNames(Word id, char **optionName)
{
    menuItems[id].optionName = optionName;
}

static void setMenuItemWithOptionNames(Word id, int posX, int posY, char *label, bool centered, Word muiStyle, Word *optionValuePtr, Word optionsRange, char **optionName)
{
    setMenuItem(id, posX, posY, label, centered, muiStyle, optionValuePtr, optionsRange);
    setMenuItemOptionNames(id, optionName);
}

static void setItemPage(Word id, int page)
{
    itemPage[id] = page;
}

static void setItemPageRange(Word fromId, Word toId, Word page)
{
    Word i;
    for (i=fromId; i<=toId; ++i) {
        setItemPage(i, page);
    }
}

static void setMenuItemVisibility(Word id, bool visible)
{
    menuItems[id].show = visible;
}

static void setMenuItemLoopBehaviour(Word id, bool loops)
{
    menuItems[id].loops = loops;
}

void resetMenuOptions() // Reset some menu options every time we start a new level
{
    opt_cheatNoclip = false;
    opt_cheatAutomap = AUTOMAP_CHEAT_OFF;
}

void setPrimaryMenuOptions() // Set menu options only once at start up
{
    // Hack to match the inverted ScreenSizeOption (0 is biggest size) to the slider index logic (0 is on the left side)
    opt_screenSizeIndex = SCREENSIZE_OPTIONS_NUM - 1 - ScreenSizeOption;

    opt_fps = false;
    opt_wallQuality = WALL_QUALITY_HI;
    opt_floorQuality = FLOOR_QUALITY_HI;
    opt_depthShading = DEPTH_SHADING_ON;
    opt_thingsShading = false;
    opt_renderer = RENDERER_DOOM;
    opt_extraRender = EXTRA_RENDER_OFF;
    opt_thickLines = false;
    opt_waterFx = false;
    opt_sky = SKY_DEFAULT;
    opt_fireSkyHeight = 2;  // 0 to 3 range
    opt_cheatsRevealed = CHEATS_OFF;
    opt_cheatIDDQD = false;
    opt_cheatIDKFAdummy = false;
    opt_playerSpeed = PLAYER_SPEED_1X;
    opt_enemySpeed = ENEMY_SPEED_1X;
    opt_extraBlood = false;
    opt_fly = false;

    opt_dbg1 = 0;
    opt_dbg2 = 0;
    opt_dbg3 = 0;
    opt_dbg4 = 0;

    resetMenuOptions();
}

void initMenuOptions()
{
    setMenuItem(mi_soundVolume, 160, 40, "Sound volume", true, muiStyle_slider, &SfxVolume, AUDIOSLIDERS_OPTIONS_NUM);
    setMenuItem(mi_musicVolume, 160, 100, "Music volume", true, muiStyle_slider, &MusicVolume, AUDIOSLIDERS_OPTIONS_NUM);
    setItemPageRange(mi_soundVolume, mi_musicVolume, page_audio);

    setMenuItem(mi_controls, 160, 40, "Controls", true, muiStyle_special, &ControlType, CONTROLS_OPTIONS_NUM);
    setMenuItemLoopBehaviour(mi_controls, false);
    setItemPageRange(mi_controls, mi_controls, page_controls);

#ifdef DEBUG_MENU_HACK
    setMenuItem(mi_dbg1, 160, 40, 0, true, muiStyle_slider, &opt_dbg1, DEBUG1_OPTIONS_NUM);
    setMenuItem(mi_dbg2, 160, 80, 0, true, muiStyle_slider, &opt_dbg2, DEBUG2_OPTIONS_NUM);
    setMenuItem(mi_dbg3, 160, 120, 0, true, muiStyle_slider, &opt_dbg3, DEBUG3_OPTIONS_NUM);
    setMenuItem(mi_dbg4, 160, 160, 0, true, muiStyle_slider, &opt_dbg4, DEBUG4_OPTIONS_NUM);
    setItemPageRange(mi_dbg1, mi_dbg4, page_debug);
#endif

    setMenuItemWithOptionNames(mi_fps, 112, 36, "Fps", false, muiStyle_text, &opt_fps, OFFON_OPTIONS_NUM, offOnOptions);
    setMenuItem(mi_screenSize, 160, 58, "Screen size", true, muiStyle_slider, &opt_screenSizeIndex, SCREENSIZE_OPTIONS_NUM);
    setMenuItemWithOptionNames(mi_wallQuality, 112, 94, "Wall", false, muiStyle_text | muiStyle_slider, &opt_wallQuality, WALLQUALITY_OPTIONS_NUM, wallQualityOptions);
    setMenuItemWithOptionNames(mi_floorQuality, 96, 126, "Floor", false, muiStyle_text | muiStyle_slider, &opt_floorQuality, FLOORQUALITY_OPTIONS_NUM, floorQualityOptions);
    setItemPageRange(mi_fps, mi_floorQuality, page_performance);

    setMenuItemWithOptionNames(mi_shading_depth, 48, 40, "Depth shade", false, muiStyle_text, &opt_depthShading, DEPTHSHADING_OPTIONS_NUM, depthShadingOptions);
    setMenuItemWithOptionNames(mi_shading_items, 40, 60, "Things shade", false, muiStyle_text, &opt_thingsShading, OFFON_OPTIONS_NUM, offOnOptions);
    setMenuItemWithOptionNames(mi_renderer, 32, 80, "Renderer", false, muiStyle_text, &opt_renderer, RENDERER_OPTIONS_NUM, rendererOptions);
    setMenuItemWithOptionNames(mi_extra_render, 56, 100, "Extra", false, muiStyle_text, &opt_extraRender, EXTRA_RENDER_OPTIONS_NUM, extraRenderOptions);
    setItemPageRange(mi_shading_depth, mi_extra_render, page_rendering);

    setMenuItemWithOptionNames(mi_mapLines, 64, 40, "Map lines", false, muiStyle_text, &opt_thickLines, THICKLINES_OPTIONS_NUM, thicklinesOptions);
    setMenuItemWithOptionNames(mi_waterFx, 80, 60, "Water fx", false, muiStyle_text, &opt_waterFx, OFFON_OPTIONS_NUM, offOnOptions);
        setMenuItemVisibility(mi_waterFx, false);   // removing this in case I won't be able to fully implement it in this release
    setMenuItemWithOptionNames(mi_sky, 96, 80, "Sky", false, muiStyle_text, &opt_sky, SKY_OPTIONS_NUM, skyOptions);
    setMenuItem(mi_firesky_slider, 96, 80, 0, false, muiStyle_slider, &opt_fireSkyHeight, SKY_HEIGHTS_OPTIONS_NUM); setMenuItemVisibility(mi_firesky_slider, false);
    setItemPageRange(mi_mapLines, mi_firesky_slider, page_effects);

    setMenuItem(mi_enableCheats, 160, 40, "Enable cheats", true, muiStyle_slider, &opt_cheatsRevealed, CHEATSREVEALED_OPTIONS_NUM);
    setMenuItemWithOptionNames(mi_cheatAutomap, 96, 80, "Automap", false, muiStyle_text, &opt_cheatAutomap, AUTOMAP_OPTIONS_NUM, automapOptions);     setMenuItemVisibility(mi_cheatAutomap, false);
    setMenuItemWithOptionNames(mi_cheatNoclip, 96, 100, "Noclip", false, muiStyle_text, &opt_cheatNoclip, OFFON_OPTIONS_NUM, offOnOptions);      setMenuItemVisibility(mi_cheatNoclip, false);
    setMenuItemWithOptionNames(mi_cheatIDDQD, 96, 122, "IDDQD", false, muiStyle_text, &opt_cheatIDDQD, OFFON_OPTIONS_NUM, offOnOptions);        setMenuItemVisibility(mi_cheatIDDQD, false);
    setMenuItemWithOptionNames(mi_cheatIDKFA, 96, 144, "IDKFA", false, muiStyle_text, &opt_cheatIDKFAdummy, DUMMYIDKFA_OPTIONS_NUM, dummyIDKFAoptions);        setMenuItemVisibility(mi_cheatIDKFA, false);
    setItemPageRange(mi_enableCheats, mi_cheatIDKFA, page_cheats);

    setMenuItemWithOptionNames(mi_playerSpeed, 60, 40, "Player speed", false, muiStyle_text, &opt_playerSpeed, PLAYER_SPEED_OPTIONS_NUM, playerSpeedOptions);
    setMenuItemWithOptionNames(mi_enemySpeed, 60, 70, "Enemy speed", false, muiStyle_text, &opt_enemySpeed, ENEMY_SPEED_OPTIONS_NUM, enemySpeedOptions);
    setMenuItemWithOptionNames(mi_extraBlood, 60, 100, "Extra blood", false, muiStyle_text, &opt_extraBlood, OFFON_OPTIONS_NUM, offOnOptions);
    setMenuItemWithOptionNames(mi_flyMode, 60, 130, "Fly mode", false, muiStyle_text, &opt_fly, OFFON_OPTIONS_NUM, offOnOptions);
    setItemPageRange(mi_playerSpeed, mi_flyMode, page_extra);
}

/*********************************

	Init the button settings from the control type

**********************************/

static void SetButtonsFromControltype(void)
{
	Word *TablePtr;

    TablePtr = &configuration[ControlType][0];	// Init table
	PadSpeed = TablePtr[0];		// Init the joypad settings
	PadAttack =	TablePtr[1];
	PadUse = TablePtr[2];
	SetSfxVolume(SfxVolume);		// Set the system volumes
	SetMusicVolume(MusicVolume);	// Set the music volume
	InitMathTables();				// Handle the math tables
}

/*********************************

	Init the option screens
	Called on powerup.

**********************************/

static int getOptionLabelRealPosX(Word id)
{
    MenuItem *mi = &menuItems[id];
    int realPosX = mi->posX;

    if (mi->centered) realPosX -= (GetBigStringWidth((Byte*)mi->label) >> 1);
    return realPosX;
}

static int getOptionLabelRealEndPosX(Word id)
{
    MenuItem *mi = &menuItems[id];

    return getOptionLabelRealPosX(id) + GetBigStringWidth((Byte*)mi->label);
}

void O_Init(void)
{
// The prefs has set controltype, so set buttons from that

	SetButtonsFromControltype();		// Init the joypad settings
	cursorCount = 0;		// Init skull cursor state
	cursorFrame = 0;
	cursorPos = 0;
	toggleIDKFAcount = 0;

    initMenuOptions();
}

/********************************

	Button bits can be eaten by clearing them in JoyPadButtons
	Called by player code.

*********************************/

static void handleCheatsMenuVisibility()
{
    Word i;
    bool cheatsVisible[4] = {false, false, false, false};

    for (i=0; i<2*opt_cheatsRevealed; ++i) {
        cheatsVisible[i] = true;
    }

    for (i=0; i<4; ++i) {
        setMenuItemVisibility(mi_cheatAutomap + i, cheatsVisible[i]);
    }
}

static void drawMenuItemControls()
{
    MenuItem *mi = &menuItems[mi_controls];
    int jposX = mi->posX - 70;

    PrintBigFont(jposX+10,mi->posY+20,(Byte*)"A");
    PrintBigFont(jposX+10,mi->posY+40,(Byte*)"B");
    PrintBigFont(jposX+10,mi->posY+60,(Byte*)"C");
    PrintBigFont(jposX+40,mi->posY+20,(Byte*)buttona[ControlType]);
    PrintBigFont(jposX+40,mi->posY+40,(Byte*)buttonb[ControlType]);
    PrintBigFont(jposX+40,mi->posY+60,(Byte*)buttonc[ControlType]);
}

static void handleSpecialActionsIfOptionChanged(player_t *player)
{
    MenuItem *mi = &menuItems[cursorPos];

    if (!mi->changed) return;

    switch(cursorPos)
    {
        case mi_soundVolume:
            S_StartSound(0,sfx_pistol);
            SetSfxVolume(SfxVolume);
        break;

        case mi_musicVolume:
        {
            if (MusicVolume==0) {
                S_StopSong();
                isMusicStopped = true;
            } else {
                if (isMusicStopped) {
                    if (!player) {  // if player is null, we are in main menu before game even starts
                        S_StartSong(Song_intro, TRUE);
                    }
                    else {
                        S_StartSong(Song_e1m1-1+gamemap, TRUE);
					}
                    isMusicStopped = false;
                }
                SetMusicVolume(MusicVolume);
            }
        }
        break;

        case mi_screenSize:
            ScreenSizeOption = SCREENSIZE_OPTIONS_NUM-1 - opt_screenSizeIndex;  // ScreenSizeOption is reversed from screenSizeIndex
            if (player) {
                InitMathTables();
            }
            initCCBarraySky();
        break;

        case mi_wallQuality:
            initCCBarrayWall();
        break;

        case mi_floorQuality:
            initSpanDrawFunc();
        break;

        case mi_sky:
            setMenuItemVisibility(mi_firesky_slider, (opt_sky==SKY_PLAYSTATION));
        break;

        case mi_firesky_slider:
            updateFireSkyHeightPal();
        break;

        case mi_enableCheats:
            handleCheatsMenuVisibility();
        break;

        case mi_cheatAutomap:
            ShowAllThings = (opt_cheatAutomap & AUTOMAP_CHEAT_SHOWTHINGS);
            ShowAllLines = (opt_cheatAutomap & AUTOMAP_CHEAT_SHOWLINES) >> 1;
            S_StartSound(0,sfx_rxplod);
        break;

        case mi_cheatNoclip:
            toggleNoclip(player);
            S_StartSound(0,sfx_rxplod);
        break;

        case mi_cheatIDDQD:
            toggleIDDQD(player);
            S_StartSound(0,sfx_rxplod);
        break;

        case mi_cheatIDKFA:
            applyIDKFA(player);
            S_StartSound(0,sfx_rxplod);
        break;

        case mi_flyMode:
            toggleFlyMode(player);
        break;

        default:
            // no special behaviour
        break;
    }
    mi->changed = false;
}

static void handleInputOptionChanges(Word buttons)
{
    MenuItem *mi = &menuItems[cursorPos];

    Word val = *mi->optionValuePtr;
    Word num = mi->optionsRange;

    if (mi->loops)  // Alternative style of activation, pressing A will toggle for options that have no slider
    {
        if (buttons & PadLeft) {
            if (val-- == 0) val = num - 1;
            mi->changed = true;
        }
        if ((buttons & PadRight) || (buttons & PadA)) {
            if (++val >= num) val = 0;
            mi->changed = true;
        }
    } else {    // But options with slider will have min and max limits, won't loop
        if (buttons & PadLeft) {
            if (val > 0) {
                --val;
                mi->changed = true;
            }
        }
        if (buttons & PadRight) {
            if (val < num-1) {
                ++val;
                mi->changed = true;
            }
        }
    }
    if (mi->changed) *mi->optionValuePtr = val;
}

static void handleInputMoveThroughOptions(Word buttons)
{
    if (buttons & PadDown) {
        do {
            ++cursorPos;
            if (cursorPos >= NUM_MENUITEMS) cursorPos = 0;
        } while (!menuItems[cursorPos].show);
        S_StartSound(0,sfx_itemup);
    }
    if (buttons & PadUp) {
        do {
            if (cursorPos == 0) cursorPos = NUM_MENUITEMS;
            --cursorPos;
        } while (!menuItems[cursorPos].show);
        S_StartSound(0,sfx_itemup);
    }
}

void O_Control(player_t *player)
{
	Word buttons;

	buttons = JoyPadButtons;

	if (NewJoyPadButtons & PadX) {		// Toggled the option screen?
		if (player) {
			player->AutomapFlags ^= AF_OPTIONSACTIVE;	// Toggle the flag
			if (!(player->AutomapFlags & AF_OPTIONSACTIVE)) {	// Shut down?
				SetButtonsFromControltype();	// Set the memory
				WritePrefsFile();		// Save new settings to NVRAM
			}
		} else {
			SetButtonsFromControltype();	// Set the memory
			WritePrefsFile();		// Save new settings to NVRAM
		}
	}

	if (player) {
		if ( !(player->AutomapFlags & AF_OPTIONSACTIVE) ) {	// Can I execute?
			return;		// Exit NOW!
		}
		menuItems[mi_enableCheats].show = true;
		menuItems[mi_flyMode].show = true;
	} else {
        // If player is off (meaning I guess before starting a game, disable the cheats menu)
	    menuItems[mi_enableCheats].show = false;
	    menuItems[mi_flyMode].show = false;
	}

// Clear buttons so game player isn't moving around

	JoyPadButtons = buttons&PadX;	// Leave option status alone

// animate skull

	cursorCount += ElapsedTime;
	if (cursorCount >= (TICKSPERSEC/4)) {	// Time up?
		cursorFrame ^= 1;		// Toggle the frame
		cursorCount = 0;		// Reset the timer
	}

// Check for movement

	if (! (buttons & (PadUp|PadDown|PadLeft|PadRight|PadA) ) ) {
		moveCount = TICKSPERSEC;		// move immediately on next press
	} else {
		moveCount += ElapsedTime;
        if (moveCount >= (TICKSPERSEC/5)) {
			moveCount = 0;		// Reset timer

			// Try to move the cursor up or down...
			handleInputMoveThroughOptions(buttons);

			// if user tries to change options with the input, we need to update these values
            handleInputOptionChanges(buttons);

            // Some of the option changes might need to trigger additional actions
            handleSpecialActionsIfOptionChanged(player);
		}
	}
}

/*********************************

	Draw the option screen

**********************************/

static void drawMenuItemLabel(Word id)
{
    MenuItem *mi = &menuItems[id];

    void (*printLabelFunc)(Word, Word, Byte*) = PrintBigFont;
    if (mi->centered) printLabelFunc = PrintBigFontCenter;

    printLabelFunc(mi->posX, mi->posY, (Byte*)mi->label);
}

static void drawMenuItemOption(Word id)
{
    MenuItem *mi = &menuItems[id];

    if (mi->optionName == 0) return;

    if (mi->optionsRange > 0)
        PrintBigFont(getOptionLabelRealEndPosX(id) + OPTION_OFFSET_X, mi->posY, (Byte*)(mi->optionName[*mi->optionValuePtr]));
}

static void drawMenuItemSlider(Word id)
{
    MenuItem *mi = &menuItems[id];

    Word sliderY = mi->posY + 20;
    int sliderHandleOffsetX = 0;

    if (mi->optionsRange > 1) {
        sliderHandleOffsetX = (SLIDER_WIDTH * (*mi->optionValuePtr)) / (mi->optionsRange - 1);
    }

    DrawMShape(SLIDER_POSX, sliderY, GetShapeIndexPtr(sliderShapes, BAR));
    DrawMShape(SLIDER_POSX + 5 + sliderHandleOffsetX, sliderY, GetShapeIndexPtr(sliderShapes, HANDLE));
}

static void drawSpecialMenuOptions(Word id)
{
    switch(id)
    {
        case mi_controls:
            drawMenuItemControls();
        break;

        default:
            // Not sure if we will ever have another one
        break;
    }
}

static void drawMenuItem(Word id)
{
    MenuItem *mi = &menuItems[id];
    Word style = mi->muiStyle;

    drawMenuItemLabel(id);

    if (mi->muiStyle == muiStyle_special) {
        drawSpecialMenuOptions(id);
    } else {
        if (style & muiStyle_text)
            drawMenuItemOption(id);
        if (style & muiStyle_slider)
            drawMenuItemSlider(id);
    }
}

static void handleSpecialMenuItemDrawingUpdates(Word id)
{
    switch(id)
    {
        case mi_cheatAutomap:
            *menuItems[id].optionValuePtr = ShowAllThings | (ShowAllLines << 1);
        break;

        case mi_cheatIDKFA:
            if (*menuItems[id].optionValuePtr == 1) {
                toggleIDKFAcount += ElapsedTime;
                if (toggleIDKFAcount >= (TICKSPERSEC/2)) {
                    toggleIDKFAcount = 0;
                    *menuItems[id].optionValuePtr = 0;
                }
            }
        break;

        default:
        break;
    }
}

static void drawMenuPage()
{
    Word i;
    Word page = itemPage[cursorPos];

    // Draw Page label
    PrintBigFontCenter(160, 10, (Byte*)pageLabel[page]);

    // Draw Page items
    for (i=0; i<NUM_MENUITEMS; ++i) {
        if (itemPage[i] == page && menuItems[i].show) {
            handleSpecialMenuItemDrawingUpdates(i);
            drawMenuItem(i);
        }
    }
}

void O_Drawer(void)
{
    int cursorPosX = getOptionLabelRealPosX(cursorPos) - 32;
    int cursorPosY = menuItems[cursorPos].posY - 2;

    // Special case for naked slider without label or options
    if (menuItems[cursorPos].muiStyle == muiStyle_slider && !menuItems[cursorPos].label) {
        cursorPosX = SLIDER_POSX - 32;
        cursorPosY += 20;
    }

    // Erase old and Draw new cursor frame

	DrawMShape(cursorPosX, cursorPosY, GetShapeIndexPtr(LoadAResource(rSKULLS),cursorFrame));
	ReleaseAResource(rSKULLS);
	sliderShapes = LoadAResource(rSLIDER);

    drawMenuPage();

	ReleaseAResource(rSLIDER);
	UpdateAndPageFlip();
}
