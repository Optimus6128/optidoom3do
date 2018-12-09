#include "Doom.h"
#include "stdio.h"

#include <Portfolio.h>
#include <event.h>
#include <Init3do.h>
#include <celutils.h>

static int SkyHeights[6] = {
	160,
	144,
	128,
	112,
	96,
	80
};

#define GRADIENT_SKY_HEIGHT 128
#define GRADIENT_SKIES_NUM 4
CCB *gradientSkyCel[GRADIENT_SKIES_NUM];
uint16 gradientSkyBmp[GRADIENT_SKIES_NUM * GRADIENT_SKY_HEIGHT];

#define FIRESKY_PIECES 4
#define FIRESKY_WIDTH 32
#define FIRESKY_HEIGHT 128
CCB *fireSkyCel;
uint8 fireSkyBmp[FIRESKY_WIDTH * FIRESKY_HEIGHT];

#define SKY_COLORS_NUM 32
#define SKY_HEIGHT_PALS 4
uint16 fireSkyPal[SKY_HEIGHT_PALS * SKY_COLORS_NUM];

Word fireSkyHeight = 2;

#define RANDTAB_SIZE 1024   // must be power of two
static uint8 randTab[RANDTAB_SIZE];
static uint8 nextRandIndex = 0;

// Helper function to get the sky height in pixels in order to erase or replace the exact portion of the screen with the new type of skies
int getSkyHeight(unsigned int i)
{
    if (i >= SCREENSIZE_OPTIONS_NUM) i = SCREENSIZE_OPTIONS_NUM-1;
    return SkyHeights[i];
}

void setSkyColors(int r0,int g0,int b0, int r1,int g1,int b1, int r2,int g2,int b2, int r3,int g3,int b3, int r4,int g4,int b4, uint16 *bmp)
{
    const int qHeight = GRADIENT_SKY_HEIGHT/4;
    setColorGradient16(0,qHeight, r0,g0,b0, r1,g1,b1, bmp);
    setColorGradient16(qHeight,(2*qHeight), r1,g1,b1, r2,g2,b2, bmp);
    setColorGradient16(2*qHeight,(3*qHeight), r2,g2,b2, r3,g3,b3, bmp);
    setColorGradient16(3*qHeight,(4*qHeight)-1, r3,g3,b3, r4,g4,b4, bmp);
}

void initNewSkies()
{
    int i, x, y;
    int lowSkyCol = 8;

    for (i=0; i<GRADIENT_SKIES_NUM; ++i) {
        gradientSkyCel[i] = CreateCel(GRADIENT_SKY_HEIGHT, 1, 16, CREATECEL_UNCODED, &gradientSkyBmp[GRADIENT_SKY_HEIGHT * i]);
        gradientSkyCel[i]->ccb_XPos = 0;
        gradientSkyCel[i]->ccb_YPos = 0;
        // Will provide HDY later, depending on screen size
        gradientSkyCel[i]->ccb_HDX = 0;
        gradientSkyCel[i]->ccb_VDY = 0;
        // Will provide VDX later, depending on screen size
    }

    for (i=0; i<GRADIENT_SKIES_NUM; ++i) {
        uint16 *bmp = &gradientSkyBmp[GRADIENT_SKY_HEIGHT * i];
        int skyId = SKY_GRADIENT_DAY + i;

        switch(skyId) {
            case SKY_GRADIENT_DAY:
                setSkyColors(64,128,208, 80,136,208, 96,160,216, 128,176,224, 176,216,232, bmp);
            break;

            case SKY_GRADIENT_NIGHT:
                setSkyColors(0,0,16, 0,0,24, 0,0,32, 0,0,48, 0,0,56, bmp);
            break;

            case SKY_GRADIENT_DUSK:
                setSkyColors(16,24,32, 32,32,48, 64,48,72, 112,96,64, 192,160,48, bmp);
            break;

            case SKY_GRADIENT_DAWN:
                setSkyColors(24,48,128, 64,64,128, 96,72,128, 144,80,96, 224,128,64, bmp);
            break;

            default:
            break;
        }
    }

    fireSkyCel = CreateCel(FIRESKY_WIDTH, FIRESKY_HEIGHT, 8, CREATECEL_CODED, fireSkyBmp);
    fireSkyCel->ccb_YPos = 0;
    fireSkyCel->ccb_HDY = 0;
    fireSkyCel->ccb_VDX = 0;

    for (i=0; i<SKY_HEIGHT_PALS; ++i) {
        int skyI = (SKY_HEIGHT_PALS - i - 1) * lowSkyCol;
        uint16 *currentPal = &fireSkyPal[SKY_COLORS_NUM * i];
        int colStep = (SKY_COLORS_NUM - skyI) / SKY_HEIGHT_PALS;
        setColorGradient16(0, skyI-1, 8,8,8, 8,8,8, currentPal);
        setColorGradient16(skyI, skyI+colStep, 8,8,8, 144,40,8, currentPal); skyI += colStep;
        setColorGradient16(skyI, skyI+colStep, 144,40,8, 216,136,8, currentPal); skyI += colStep;
        setColorGradient16(skyI, skyI+colStep, 216,136,8, 200,152,32, currentPal); skyI += colStep;
        setColorGradient16(skyI, 31, 200,152,32, 255,255,255, currentPal);
    }

    updateFireSkyHeightPal();

    i = 0;
    for (y=0; y<FIRESKY_HEIGHT; ++y) {
        for (x=0; x<FIRESKY_WIDTH; ++x) {
            fireSkyBmp[i++] = (x+y) & 31;
        }
    }

    // Clean up fire sky buffer and fill fire base with max
    memset(fireSkyBmp, 0, (FIRESKY_HEIGHT-1) * FIRESKY_WIDTH);
    memset(&fireSkyBmp[(FIRESKY_HEIGHT-1) * FIRESKY_WIDTH], 31, FIRESKY_WIDTH);

	for (i = 0; i < RANDTAB_SIZE; ++i) {
		randTab[i] = (uint8)(rand() % 3);
	}
}

static void updateFireCelBmp()
{
    int x, y, randX;
    uint8 c, *src;

	for (x = 0; x < FIRESKY_WIDTH; ++x) {
		src = &fireSkyBmp[2*FIRESKY_WIDTH + x];
		for (y = 2; y < FIRESKY_HEIGHT; ++y) {
            c = *src;
            src -= FIRESKY_WIDTH;
            if (c == 0) {
                *src = 0;
            } else {
                randX = randTab[nextRandIndex];
                *(src + randX - 1) = c - (randX & 1);
                nextRandIndex = (nextRandIndex + 1) & (RANDTAB_SIZE - 1);
            }
			src += 2*FIRESKY_WIDTH;
		}
	}
}

static void drawFireSkyCels()
{
    int i;
    int skyPieceWidth = ScreenWidth >> 2;
    int skyPieceScaleX = (skyPieceWidth << 20) / FIRESKY_WIDTH;
    int skyPieceScaleY = (ScreenHeight << 16) / getSkyHeight(ScreenSizeOption);

    int playerAngleShift = ((xtoviewangle[0]+viewangle)>>ANGLETOSKYSHIFT) % skyPieceWidth;

    int skyPieceCurrentX = -skyPieceWidth;
    for (i=0; i<FIRESKY_PIECES + 1; ++i) {
        fireSkyCel->ccb_XPos = (skyPieceCurrentX + playerAngleShift) << 16;
        fireSkyCel->ccb_HDX = skyPieceScaleX;
        fireSkyCel->ccb_VDY = skyPieceScaleY;
        skyPieceCurrentX += skyPieceWidth;

        AddCelToCurrentCCB(fireSkyCel);
    }
}

static void drawFireSky()
{
    updateFireCelBmp();
    drawFireSkyCels();
}

void updateFireSkyHeightPal()
{
    fireSkyCel->ccb_PLUTPtr = (PLUTChunk*)&fireSkyPal[SKY_COLORS_NUM * fireSkyHeight];
}

void drawNewSky(int which)
{
    switch(which) {
        case SKY_GRADIENT_DAY:
        case SKY_GRADIENT_DAWN:
        case SKY_GRADIENT_NIGHT:
        case SKY_GRADIENT_DUSK:
        {
            const int gradSkyId = which - SKY_GRADIENT_DAY;
            CCB *skyCel = gradientSkyCel[gradSkyId];
            skyCel->ccb_HDY = getSkyScale(ScreenSizeOption);
            skyCel->ccb_VDX = ScreenWidth << 16;

            AddCelToCurrentCCB(skyCel);
        }
        break;

        case SKY_PLAYSTATION:
            drawFireSky();
        break;

        default:
            DrawARect(0,0, ScreenWidth, getSkyHeight(ScreenSizeOption), 0);
        break;
    }
}
