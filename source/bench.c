#include "Doom.h"
#include "bench.h"

#include "stdio.h"

#include <timerutils.h>


int nframe = 0, pframe = 0, ptime = 0;

static char FPSstr[6] = "";

Item timerIOreq;

void initTimer(void)
{
    timerIOreq = GetTimerIOReq();
}

int getTicks(void)
{
    return GetMSecTime(timerIOreq);
}

uTime getUTime(void)
{
	static uTime t;

	GetUSecTime(timerIOreq, &t.seconds, &t.useconds);
	return t;
}

static void updateFPSstring(int fps)
{
	const int fps_int = fps / 10;
	const int fps_frac = fps - 10 * fps_int;

	sprintf(FPSstr, "%d.%d", fps_int, fps_frac);
}

static int updateAndGetFPS(void)
{
    static int fps = 0;
	const int dt = getTicks() - ptime;
    if (dt >= 1000)
    {
        fps = ((nframe - pframe) * 10000) / dt;
        pframe = nframe;

		updateFPSstring(fps);
        ptime = getTicks();
    }
    return fps;
}

void printFPS(int px, int py)
{
	updateAndGetFPS();
	PrintBigFont(px, py, (Byte*)FPSstr);
}
