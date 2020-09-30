#include "Doom.h"
#include "bench.h"

#include <timerutils.h>


int nframe = 0, pframe = 0, ptime = 0;

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

int updateAndGetFPS(void)
{
    static int fps = 0;
    if (getTicks() - ptime >= 1000)
    {
        ptime = getTicks();
        fps = nframe - pframe;
        pframe = nframe;
    }
    return fps;
}
