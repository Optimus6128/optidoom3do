#include "doom.h"

#include <Init3do.h>


#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define FONT_SIZE (FONT_WIDTH * FONT_HEIGHT)

#define MAX_STRING_LENGTH 64
#define NUM_FONTS 59


static unsigned char bitfonts[] = {0,0,0,0,0,0,0,0,4,12,8,24,16,0,32,0,10,18,20,0,0,0,0,0,0,20,126,40,252,80,
0,0,6,25,124,32,248,34,28,0,4,12,72,24,18,48,32,0,14,18,20,8,21,34,29,0,32,32,64,0,0,0,
0,0,16,32,96,64,64,64,32,0,4,2,2,2,6,4,8,0,8,42,28,127,28,42,8,0,0,4,12,62,24,16,
0,0,0,0,0,0,0,0,32,64,0,0,0,60,0,0,0,0,0,0,0,0,0,0,32,0,4,12,8,24,16,48,
32,0,14,17,35,77,113,66,60,0,12,28,12,8,24,16,16,0,30,50,4,24,48,96,124,0,28,50,6,4,2,98,
60,0,2,18,36,100,126,8,8,0,15,16,24,4,2,50,28,0,14,17,32,76,66,98,60,0,126,6,12,24,16,48,
32,0,56,36,24,100,66,98,60,0,14,17,17,9,2,34,28,0,0,0,16,0,0,16,0,0,0,0,16,0,16,32,
0,0,0,0,0,0,0,0,0,0,0,0,30,0,60,0,0,0,0,0,0,0,0,0,0,0,28,50,6,12,8,0,
16,0,0,0,0,0,0,0,0,0,14,27,51,63,99,65,65,0,28,18,57,38,65,65,62,0,14,25,32,96,64,98,
60,0,12,18,49,33,65,66,60,0,30,32,32,120,64,64,60,0,31,48,32,60,96,64,64,0,14,25,32,96,68,98,
60,0,17,17,50,46,100,68,68,0,8,8,24,16,48,32,32,0,2,2,2,6,68,68,56,0,16,17,54,60,120,76,
66,0,16,48,32,96,64,64,60,0,10,21,49,33,99,66,66,0,17,41,37,101,67,66,66,0,28,50,33,97,67,66,
60,0,28,50,34,36,120,64,64,0,28,50,33,97,77,66,61,0,28,50,34,36,124,70,66,0,14,25,16,12,2,70,
60,0,126,24,16,16,48,32,32,0,17,49,35,98,70,68,56,0,66,102,36,44,40,56,48,0,33,97,67,66,86,84,
40,0,67,36,24,28,36,66,66,0,34,18,22,12,12,8,24,0,31,2,4,4,8,24,62,0};

static CCB *textCel[MAX_STRING_LENGTH];

static uchar fontsBmp[NUM_FONTS * FONT_SIZE];
static uint16 fontsPal[32];
static uchar fontsMap[256];


bool loadPsxSamples = false;

static void initFonts()
{
    int i = 0;
    int n, x, y;

    for (n=0; n<32; ++n) {
        fontsPal[n] = MakeRGB15(n, n, n);
    }

	for (n=0; n<59; n++) {
		for (y=0; y<8; y++) {
			int c = bitfonts[i++];
			for (x=0; x<8; x++) {
				fontsBmp[(n << 6) + x + (y<<3)] = ((c >>  (7 - x)) & 1) * 31;
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
            c = 255;

        fontsMap[i] = c;
	}

    for (i=0; i<MAX_STRING_LENGTH; ++i) {
        textCel[i] = CreateCel(FONT_WIDTH, FONT_HEIGHT, 8, CREATECEL_CODED, fontsBmp);
        textCel[i]->ccb_PLUTPtr = (PLUTChunk*)fontsPal;

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
    fontsPal[31] = MakeRGB15(r,g,b);
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

void startModMenu()
{
    bool exit = false;

    int cursorIndexY = 0;

    const int cursorIndexMax = 1;
    const int cursorPosX = 40;
    const int cursorPosYoffset = -2;
    int cursorPosY;

    const int menuXstart = 96;
    const int menuYstart = 64;
    const int menuFinishOptionY = menuYstart + 16;

    char *soundFxModLabel[2] = { "ORIGINAL", "PSX" };


    initFonts();

    do {
        int t;
        Word buttons = ReadJoyButtons(0);

        if (buttons & PadUp) {
            if (cursorIndexY > 0) --cursorIndexY;
        }

        if (buttons & PadDown) {
            if (cursorIndexY < cursorIndexMax) ++ cursorIndexY;
        }

        if (cursorIndexY==0) {
            if ((buttons & PadA) | (buttons & PadLeft) | (buttons & PadRight)) {
                loadPsxSamples = !loadPsxSamples;
            }
        }

        if (cursorIndexY==cursorIndexMax) {
            if (buttons & PadA) exit = true;
        }

        if (buttons & PadStart) exit = true;


        DrawARect(0, 0, 320, 240, 0x000F); FlushCCBs();

        for (t=0; t<2; ++t) {
            setFontColor(15<<t,15<<t,15<<t);
            drawZoomedText(64-t, 16-t, "MOD OPTIONS", 512);
        }

        setFontColor(7,15,31);
        drawText(menuXstart, menuYstart, "SOUND FX:");

        setFontColor(15,23,31);
        drawText(menuXstart + 80, menuYstart, soundFxModLabel[(int)loadPsxSamples]);

        setFontColor(31,15,0);
        drawText(menuXstart + 32, menuFinishOptionY, "start!");

        cursorPosY = menuYstart + (cursorIndexY << 4);
        if (cursorIndexY == cursorIndexMax) cursorPosY = menuFinishOptionY;
        cursorPosY += cursorPosYoffset;

        setFontColor(31,31,15);
        drawText(cursorPosX, cursorPosY, "*");


        updateScreenAndWait();

        t = getTicks();
        while(getTicks() - t < 125) {};

    } while(!exit);
}
