#include "Doom.h"
#include "stdio.h"
#include "bench.h"

#include <Portfolio.h>
#include <event.h>
#include <Init3do.h>
#include <celutils.h>

/*
#define GRADIENT_SKY_HEIGHT 128
#define GRADIENT_SKIES_NUM 4
CCB *gradientSkyCel[GRADIENT_SKIES_NUM];
uint16 *gradientSkyBmp;
*/

#define FIRESKY_PIECES 4
#define FIRESKY_WIDTH 32
#define FIRESKY_HEIGHT 128
CCB *fireSkyCel;
uint8 *fireSkyBmp;

#define SKY_COLORS_NUM 32
#define SKY_HEIGHT_PALS 4
uint16 *fireSkyPal;

#define RANDTAB_OFFSET 256 // power of two
#define RANDTAB_SIZE (FIRESKY_WIDTH * FIRESKY_HEIGHT + RANDTAB_OFFSET)
static uint8 *randTab;

static bool fireSkyIsInit = false;

int getSkyScale()
{
    return (Fixed)(1048576.0*((float)ScreenHeight/160.0));
}

/*void setSkyColors(int r0,int g0,int b0, int r1,int g1,int b1, int r2,int g2,int b2, int r3,int g3,int b3, int r4,int g4,int b4, uint16 *bmp)
{
    const int qHeight = GRADIENT_SKY_HEIGHT/4;
    setColorGradient16(0,qHeight, r0,g0,b0, r1,g1,b1, bmp);
    setColorGradient16(qHeight,(2*qHeight), r1,g1,b1, r2,g2,b2, bmp);
    setColorGradient16(2*qHeight,(3*qHeight), r2,g2,b2, r3,g3,b3, bmp);
    setColorGradient16(3*qHeight,(4*qHeight)-1, r3,g3,b3, r4,g4,b4, bmp);
}

static void initGradientSkies()
{
    gradientSkyBmp = (uint16*)AllocAPointer(2 * GRADIENT_SKIES_NUM * GRADIENT_SKY_HEIGHT);

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
}*/

void initFireSky()
{
	skyOnView = true;	// force to true so that in first frame during doom wipe, will clear the garbage before manually set to true if sky is visible during game logic.

	if (!fireSkyIsInit) {
		int i;
		const int lowSkyCol = 8;

		fireSkyBmp = (uint8*)AllocAPointer(FIRESKY_WIDTH * FIRESKY_HEIGHT);
		fireSkyPal = (uint16*)AllocAPointer(2 * SKY_HEIGHT_PALS * SKY_COLORS_NUM);
		randTab = (uint8*)AllocAPointer(RANDTAB_SIZE);

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

		// Clean up fire sky buffer and fill fire base with max
		memset(fireSkyBmp, 0, (FIRESKY_HEIGHT-1) * FIRESKY_WIDTH);
		memset(&fireSkyBmp[(FIRESKY_HEIGHT-1) * FIRESKY_WIDTH], 31, FIRESKY_WIDTH);

		for (i = 0; i < RANDTAB_SIZE; ++i) {
			randTab[i] = (uint8)(rand() % 3);
		}

		fireSkyIsInit = true;
	}
}

void deinitFireSky()
{
	if (fireSkyIsInit) {
		DeallocAPointer(fireSkyBmp);
		DeallocAPointer(fireSkyPal);
		DeallocAPointer(randTab);
		fireSkyIsInit = false;
	}
}

static void updateFireCelBmp()
{
	int x, y;
	uint8 c, *src;
	uint8 *rtab = &randTab[getTicks() & (RANDTAB_OFFSET-1)];

	for (x = 0; x < FIRESKY_WIDTH; ++x) {
		src = &fireSkyBmp[2*FIRESKY_WIDTH + x];
		for (y = 2; y < FIRESKY_HEIGHT; ++y) {
			c = *src;
			src -= FIRESKY_WIDTH;
			if (c == 0) {
				*src = 0;
			} else {
				int randX = *rtab++;
				*(src + randX - 1) = c - (randX & 1);
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
    int skyPieceScaleY = (ScreenHeight << 16) / 160;

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
    fireSkyCel->ccb_PLUTPtr = &fireSkyPal[SKY_COLORS_NUM * optOther->fireSkyHeight];
}

void drawNewSky(int which)
{
    switch(which) {
        /*case SKY_GRADIENT_DAY:
        case SKY_GRADIENT_DAWN:
        case SKY_GRADIENT_NIGHT:
        case SKY_GRADIENT_DUSK:
        {
            const int gradSkyId = which - SKY_GRADIENT_DAY;
            CCB *skyCel = gradientSkyCel[gradSkyId];
            skyCel->ccb_HDY = getSkyScale();
            skyCel->ccb_VDX = ScreenWidth << 16;

            AddCelToCurrentCCB(skyCel);
        }
        break;*/

        case SKY_PLAYSTATION:
            drawFireSky();
        break;

        default:
            DrawARect(0,0, ScreenWidth, ScreenHeight, 0);
        break;
    }
}
