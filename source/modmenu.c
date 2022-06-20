#include "doom.h"
#include "bench.h"
#include "stdio.h"
#include "string.h"

#include <Init3do.h>

#include "filesystem.h"
#include "filefunctions.h"
#include "directory.h"
#include "directoryfunctions.h"

#include "wad_loader.h"
#include "input.h"
#include "tools.h"

#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define FONT_SIZE (FONT_WIDTH * FONT_HEIGHT)

#define MAX_STRING_LENGTH 41
#define NUM_FONTS 60

static unsigned char bitfonts[8 * NUM_FONTS] = {0,0,0,0,0,0,0,0,
4,12,8,24,16,0,32,0,
10,18,20,0,0,0,0,0,
0,20,126,40,252,80,0,0,
6,25,124,32,248,34,28,0,
4,12,72,24,18,48,32,0,
14,18,20,8,21,34,29,0,
32,32,64,0,0,0,0,0,
16,32,96,64,64,64,32,0,
4,2,2,2,6,4,8,0,
8,42,28,127,28,42,8,0,
0,4,12,62,24,16,0,0,
0,0,0,0,0,0,32,64,
0,0,0,60,0,0,0,0,
0,0,0,0,0,0,32,0,
4,12,8,24,16,48,32,0,

14,17,35,77,113,66,60,0,
12,28,12,8,24,16,16,0,
30,50,4,24,48,96,124,0,
28,50,6,4,2,98,60,0,
2,18,36,100,126,8,8,0,
15,16,24,4,2,50,28,0,
14,17,32,76,66,98,60,0,
126,6,12,24,16,48,32,0,
56,36,24,100,66,98,60,0,
14,17,17,9,2,34,28,0,

0,0,16,0,0,16,0,0,
0,0,16,0,16,32,0,0,
0,0,0,0,0,0,0,0,
0,0,30,0,60,0,0,0,
0,0,0,0,0,0,0,0,
28,50,6,12,8,0,16,0,
0,0,0,0,0,0,0,0,

14,27,51,63,99,65,65,0,
28,18,57,38,65,65,62,0,
14,25,32,96,64,98,60,0,
60,34,33,97,65,66,124,0,
30,32,32,120,64,64,60,0,
31,48,32,60,96,64,64,0,
14,25,32,96,68,98,60,0,
17,17,50,46,100,68,68,0,
8,8,24,16,48,32,32,0,
2,2,2,6,68,68,56,0,
16,17,54,60,120,76,66,0,
16,48,32,96,64,64,60,0,
10,21,49,33,99,66,66,0,
17,41,37,101,67,66,66,0,
28,50,33,97,67,66,60,0,
28,50,34,36,120,64,64,0,
28,50,33,97,77,66,61,0,
28,50,34,36,124,70,66,0,
14,25,16,12,2,70,60,0,
126,24,16,16,48,32,32,0,
17,49,35,98,70,68,56,0,
66,102,36,44,40,56,48,0,
33,97,67,66,86,84,40,0,
67,36,24,28,36,66,66,0,
34,18,22,12,12,8,24,0,
31,2,4,4,8,24,62,0,
0,0,0,0,0,0,0,126};


enum {
	TYPE_INT,
	TYPE_STRING
}ModMenuValueType;

typedef struct {
    char *label;
    void *values;
    int num_values;
    int type;
    int selection;
}ModMenuItem;

static CCB *textCel[MAX_STRING_LENGTH];

static uchar *fontsBmp;
static uchar *fontsMap;
static uint16 fontsPal[2];

bool loadPsxSamples;
bool enableNewSkies;
bool skipLogos;
bool debugMode;


#define NUM_OFF_ON_SELECTIONS 2
#define NUM_LOADING_FIX 3
#define NUM_SOUND_FX_SELECTIONS 2
#define NUM_VISPLANE_SELECTIONS 11

#define MAX_WAD_SELECTIONS 256

static char *wadsSelection[MAX_WAD_SELECTIONS];
static char *loadingFixSelection[NUM_LOADING_FIX] = { "OFF", "ON", "RELAXED" };
static char *offOnSelection[NUM_OFF_ON_SELECTIONS] = { "OFF", "ON" };
static char *soundFxSelection[NUM_SOUND_FX_SELECTIONS] = { "ORIGINAL", "PSX" };
static int maxVisplanesSelection[NUM_VISPLANE_SELECTIONS] = { 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96 };

static char *wadsFolder = "wads";
static DirectoryEntry *wadsDirectoryEntry;

// These 3 will be externed for later loading
static char wadSelectedFullPath[32];
char *wadSelected = NULL;

enum {
	MMOPT_MODS,
	MMOPT_LOADING_TYPE,
	MMOPT_SKIES,
	MMOPT_SOUNDFX,
	MMOPT_MAXVIS,
	MMOPT_SKIPLOGOS,
	MMOPT_DEBUG_MODE,
	MMOPT_NUM
};

static ModMenuItem mmItems[MMOPT_NUM+1] = {
	{ "MODS:", wadsSelection, 1, TYPE_STRING, 0 },
	{ "LOADING FIX:", loadingFixSelection, NUM_LOADING_FIX, TYPE_STRING, 2 },
	{ "NEW SKIES:", offOnSelection, NUM_OFF_ON_SELECTIONS, TYPE_STRING, 0 },
	{ "SOUND FX:", soundFxSelection, NUM_SOUND_FX_SELECTIONS, TYPE_STRING, 0 },
	{ "MAX VISPLANES:", maxVisplanesSelection, NUM_VISPLANE_SELECTIONS, TYPE_INT, 3 },
	{ "SKIP LOGOS:", offOnSelection, NUM_OFF_ON_SELECTIONS, TYPE_STRING, 1 },
	{ "DEBUG MODE:", offOnSelection, NUM_OFF_ON_SELECTIONS, TYPE_STRING, 0 },
	NULL
};

static char *menuItemScrollText[MMOPT_NUM] = {
	"Select and load new maps located in 'wads' folder. Doom 1 and 2 maps from PC can also be loaded (with issues) depending on the Loading Fix option\0",
	"What actions to take if a WAD has issues.   OFF: Don't load ExMx map ids, don't replace unknown textures.   ON: Replace missing textures with one default texture, map ExMx ids to MAPxx   RELAXED: map closest Doom 1/2 texture IDs to 3DO resources\0",
	"Enable new skies (gradients, fire sky). Disable to save in memory.\0",
	"Chose original or PSX sound effects. PSX sound effects sound better but  will steal more memory.\0",
	"Max Visplanes. Lower is better for memory. Higher is more suitable for high detailed maps, although chosing low visplanes will only create visual artifacts in the distance if map is too complex. Most original Doom 3DO maps don't even reach 32\0",
	"Skip those company logos that make start slower and even take some memory strangely enough\0",
	"Debug Mode ON = additional info displayed (lump name and remaining memory) during WAD loading, additional info like visplanes num when enabling Stats (instead of just FPS/Memory)\0"
};

static int scrollChar = 0;
static int scrollOffX = 320;
static char* scrollText;

int cursorIndexY = 0;
bool exit = false;


static bool getBoolFromValue(ModMenuItem *mmItem)
{
	return (bool)mmItem->selection;
}

static int getIntFromValue(ModMenuItem *mmItem)
{
	return *((int*)mmItem->values + mmItem->selection);
}

static char* getStringFromValue(ModMenuItem *mmItem)
{
	return *((char**)mmItem->values + mmItem->selection);
}

static void initFonts()
{
    int i = 0;
    int n, x, y;

	fontsBmp = (uchar*)AllocMem(NUM_FONTS * FONT_SIZE, MEMTYPE_TRACKSIZE);
	fontsMap = (uchar*)AllocMem(256, MEMTYPE_TRACKSIZE);

	for (n=0; n<NUM_FONTS; n++) {
		for (y=0; y<8; y++) {
			int c = bitfonts[i++];
			for (x=0; x<8; x++) {
				fontsBmp[(n << 6) + x + (y<<3)] = (c >>  (7 - x)) & 1;
			}
		}
	}

	for (i=0; i<256; ++i) {
        uchar c = i;

        if (c>31 && c<92)
            c-=32;
        else
            if (c>96 && c<123) c-=64;
        else
			if (c==95) c = 59;
		else
            c = 255;

        fontsMap[i] = c;
	}

	fontsPal[0] = 0;
    for (i=0; i<MAX_STRING_LENGTH; ++i) {
        textCel[i] = CreateCel(FONT_WIDTH, FONT_HEIGHT, 8, CREATECEL_CODED, fontsBmp);
        textCel[i]->ccb_PLUTPtr = fontsPal;

        textCel[i]->ccb_HDX = 1 << 20;
        textCel[i]->ccb_HDY = 0 << 20;
        textCel[i]->ccb_VDX = 0 << 16;
        textCel[i]->ccb_VDY = 1 << 16;

        textCel[i]->ccb_Flags |= (CCB_ACSC | CCB_ALSC);

        if (i > 0) LinkCel(textCel[i-1], textCel[i]);
    }
}

static void setFontColor(int r, int g, int b)
{
    fontsPal[1] = MakeRGB15(r,g,b);
}

static void drawZoomedText(int xtp, int ytp, char *text, int zoom)
{
    int i = 0;
    char c;

    do {
        c = fontsMap[*text++];

        textCel[i]->ccb_XPos = xtp  << 16;
        textCel[i]->ccb_YPos = ytp  << 16;

        textCel[i]->ccb_HDX = (zoom << 20) >> 8;
        textCel[i]->ccb_VDY = (zoom << 16) >> 8;

        textCel[i]->ccb_SourcePtr = (CelData*)&fontsBmp[c * FONT_SIZE];

        xtp+= ((zoom * 8) >> 8);
    } while(c!=255 && ++i < MAX_STRING_LENGTH);

    --i;
	textCel[i]->ccb_Flags |= CCB_LAST;
	DrawCels(VideoItem, textCel[0]);
	textCel[i]->ccb_Flags ^= CCB_LAST;
}

static void drawText(int xtp, int ytp, char *text)
{
    drawZoomedText(xtp, ytp, text, 256);
}

static void drawNumber(int xtp, int ytp, int number)
{
	static char numStr[8];
	sprintf(numStr, "%d\0", number);
	drawText(xtp, ytp, numStr);
}

static void fadeOutScanlineEffect()
{
	int y, j;
	for (j=0; j<SCREENS; ++j) {
		for (y=0; y<200; y+=8) {
			DrawARect(0, y, 320, 8, 0); FlushCCBs();
			DrawARect(0, 192 - y, 320, 8, 0); FlushCCBs();
			updateScreenAndWait();
		}
	}
}

static void alterMenuOption(int optionIndex, Word buttons)
{
	ModMenuItem *mmItem = &mmItems[optionIndex];
	int index = mmItem->selection;
	if (buttons & PadLeft) {
		--index;
		if (index < 0) index = 0;
	} else if (buttons & PadRight) {
		++index;
		if (index > mmItem->num_values-1) index = mmItem->num_values-1;
	}
	mmItem->selection = index;
}

static int getStringSize(char *str)
{
	int count = 0;
	while (*str++ != '\0' || ++count < 16) {};
	return count;
}

static void resetScroll()
{
	scrollText = menuItemScrollText[cursorIndexY];
	scrollChar = 0;
	scrollOffX = 320;
}

static void controlModMenu()
{
	if (isJoyButtonPressedOnce(JOY_BUTTON_UP)) {
		if (cursorIndexY > 0) {
			--cursorIndexY;
			resetScroll();
		}
	}

	if (isJoyButtonPressedOnce(JOY_BUTTON_DOWN)) {
		if (cursorIndexY < MMOPT_NUM) {
			++cursorIndexY;
			resetScroll();
		}
	}

	if (isJoyButtonPressedOnce(JOY_BUTTON_LEFT) | isJoyButtonPressedOnce(JOY_BUTTON_RIGHT)) {
		int i;
		for (i=0; i<MMOPT_NUM; ++i) {
			if (i==cursorIndexY) {
				alterMenuOption(i, ReadJoyButtons(0));
			}
		}
	}

	if (cursorIndexY==MMOPT_NUM) {
		if (isJoyButtonPressedOnce(JOY_BUTTON_A)) exit = true;
	}
	if (isJoyButtonPressedOnce(JOY_BUTTON_START)) exit = true;
}

static void renderModMenu()
{
    const int menuXstart = 72;
    const int menuYstart = 48;
    const int lastOptionOffset = 8;

    int cursorPosX;
    int cursorPosY;

	int i;
	int currentMenuY = menuYstart;


	DrawARect(0, 0, 320, 200, 0x000F); FlushCCBs();

	for (i=0; i<2; ++i) {
		setFontColor(15<<i,15<<i,15<<i);
		drawZoomedText(64-i, 16-i, "MOD OPTIONS", 512);
	}

	for (i=0; i<MMOPT_NUM; ++i) {
		ModMenuItem *mmItem = &mmItems[i];
		int labelOffsetX = (getStringSize(mmItem->label) + 1) * 8;

		setFontColor(7,15,31);
		drawText(menuXstart, currentMenuY, mmItem->label);
		setFontColor(15,23,31);

		switch (mmItem->type) {
			case TYPE_INT:
			{
				int *valArray = (int*)mmItem->values;
				drawNumber(menuXstart + labelOffsetX, currentMenuY, valArray[mmItem->selection]);
			}
			break;
			
			case TYPE_STRING:
			{
				char **valArray = (char**)mmItem->values;
				char *label = valArray[mmItem->selection];
				if (!label) label = "NONE";

				drawText(menuXstart + labelOffsetX, currentMenuY, label);
			}
			break;
		}
		currentMenuY+=16;
	}

	setFontColor(31,15,0);
	drawText(menuXstart + 56, currentMenuY+lastOptionOffset, "start!");

	cursorPosY = menuYstart + cursorIndexY * 16;
	if (cursorIndexY==MMOPT_NUM) {
		cursorPosX = 112;
		cursorPosY += lastOptionOffset;
	} else {
		cursorPosX = 56;
	}
	setFontColor(31,31,15);
	drawText(cursorPosX, cursorPosY, "*");

	setFontColor(31,31,15);
	if (scrollText) {
		if (scrollText[scrollChar] == 0) {
			scrollOffX = 320;
			scrollChar = 0;
		} else {
			drawText(scrollOffX, 188, &scrollText[scrollChar]);
			scrollOffX-=2;
			if (scrollOffX < -7) {
				scrollOffX = 0;
				scrollChar++;
			}
		}
	}

	updateScreenAndWait();
}

static void getWadFullPath(char *fullPathDst, const char *filename)
{
	sprintf(fullPathDst, "%s/%s", wadsFolder, filename);
}

static bool hasWadExtension(const char *filename)
{
	const char *filenameUpper = getUpperCaseStr(filename, 256);
	const char *extensionStr = strstr(filenameUpper, ".WAD");
	bool hasExtension = (extensionStr != NULL) && (extensionStr[4] == 0);
	return hasExtension;
}

static void getWadsDirectory()
{
	Directory *dir = OpenDirectoryPath(wadsFolder);
	int entriesNum = 0;

	wadsSelection[0] = NULL;

	if (dir)
	{
		DirectoryEntry *de;

		wadsDirectoryEntry = (DirectoryEntry*)AllocMem(MAX_WAD_SELECTIONS * sizeof(DirectoryEntry), MEMTYPE_TRACKSIZE);
		de = wadsDirectoryEntry;

		while (ReadDirectory(dir, de) >= 0)
		{
			if (de->de_Type != FILE_TYPE_DIRECTORY) {
				char *filename = de->de_FileName;

				if (hasWadExtension(filename)) {
					static char fullPath[32];
					getWadFullPath(fullPath, filename);
					if (isPwad(fullPath)) {
						wadsSelection[entriesNum+1] = filename;
						++de;
						++entriesNum;
					}
				}
			}
		}
		CloseDirectory(dir);
	}

    mmItems[MMOPT_MODS].num_values = entriesNum + 1;
}

void startModMenu()
{
    initInput();
    initFonts();
	initDummyCCB();

    getWadsDirectory();

	optGraphics->frameLimit = FRAME_LIMIT_VSYNC;

	scrollText = menuItemScrollText[cursorIndexY];

    do {
		updateInput();
        controlModMenu();
		renderModMenu();
    } while(!exit);

    enableNewSkies = getBoolFromValue(&mmItems[MMOPT_SKIES]);
	loadPsxSamples = getBoolFromValue(&mmItems[MMOPT_SOUNDFX]);
	skipLogos = getBoolFromValue(&mmItems[MMOPT_SKIPLOGOS]);
	maxVisplanes = getIntFromValue(&mmItems[MMOPT_MAXVIS]);
	loadingFix = mmItems[MMOPT_LOADING_TYPE].selection;
	debugMode = getBoolFromValue(&mmItems[MMOPT_DEBUG_MODE]);
	
	wadSelected = getStringFromValue(&mmItems[MMOPT_MODS]);
	resetMapLumpData();
	if (wadSelected != NULL) {
		getWadFullPath(wadSelectedFullPath, wadSelected);
		wadSelected = wadSelectedFullPath;
		loadSelectedWadLumpInfo(wadSelected);
	}

	FreeMem(fontsBmp, -1);
	FreeMem(fontsMap, -1);
	if (wadsDirectoryEntry) FreeMem(wadsDirectoryEntry, -1);

	fadeOutScanlineEffect();
}
