#include "doom.h"
#include "bench.h"

#include "stdio.h"

#include <timerutils.h>

#ifdef PROFILE_ON
	static bool doSortBenchResults = false;
	static bool displayPercentage = false;

	static BenchPeriod benchPeriods[MAX_BENCH_PERIODS];
	static BenchResult benchResults[MAX_BENCH_PERIODS];
	static BenchResult *benchResultsSorted[MAX_BENCH_PERIODS];
	static int maxBenchIndex = -1;

	static int benchFrame = 0;

	bool profilerRunning = false;
	int profileTicksStart = 0;
	int profileTicksDuration = 0;
	uTime uTimePrev;
#endif

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

#ifdef PROFILE_ON
	static uint32 getUTimeDiff(uTime *t0, uTime *t1)
	{
		return (t1->seconds - t0->seconds) * 1000000 + t1->useconds - t0->useconds;
	}

	static uTime getUTime()
	{
		static uTime t;

		GetUSecTime(timerIOreq, &t.seconds, &t.useconds);
		return t;
	}

	void clearBenchPeriods()
	{
		int i;
		for (i=0; i<MAX_BENCH_PERIODS; ++i) {
			benchPeriods[i].timeSum = 0;
			benchPeriods[i].numCalls = 0;
		}
	}

	void startBenchPeriod(int index, char *name)
	{
		if (profilerRunning) {
			if (maxBenchIndex < index) maxBenchIndex = index;
			benchPeriods[index].name = name;
			benchPeriods[index].timeStart = getUTime();
		}
	}

	void endBenchPeriod(int index)
	{
		if (profilerRunning) {
			uTime tEnd = getUTime();
			benchPeriods[index].timeSum += getUTimeDiff(&benchPeriods[index].timeStart, &tEnd);
			benchPeriods[index].numCalls++;
		}
	}

	void startProfiling(int seconds)
	{
		int i;
		for (i=0; i<MAX_BENCH_PERIODS; ++i) {
			benchResultsSorted[i] = NULL;
		}

		maxBenchIndex = -1;
		profileTicksDuration = seconds * 1000;
		profilerRunning = true;
		clearBenchPeriods();
		profileTicksStart = getTicks();
		uTimePrev = getUTime();
	}

	void stopProfiling()
	{
		profilerRunning = false;
	}

	static void sortBenchResults()
	{
		int i;
		int j = maxBenchIndex;
		
		for (i=0; i<=j; ++i) {
			benchResultsSorted[i] = &benchResults[i];
		}

		if (doSortBenchResults) {	
			bool swapped;
			do {
				swapped = false;
				for (i=1; i<=j; ++i) {
					if (benchResultsSorted[i-1]->timePerc < benchResultsSorted[i]->timePerc) {
						BenchResult *benchResultTempPtr = benchResultsSorted[i];
						benchResultsSorted[i] = benchResultsSorted[i-1];
						benchResultsSorted[i-1] = benchResultTempPtr;
						swapped = true;
					}
				}
				--j;
			} while(swapped);
		}
	}

	static void copyBenchResults()
	{
		int i;

		uTime uTimeNow = getUTime();
		uint32 timeDiff = getUTimeDiff(&uTimePrev, &uTimeNow);
		uTimePrev = uTimeNow;

		for (i=0; i<=maxBenchIndex; ++i) {
			benchResults[i].name = benchPeriods[i].name;
			if (benchFrame > 0) {
				benchResults[i].timeTics = benchPeriods[i].timeSum / benchFrame;
			} else {
				benchResults[i].timeTics = 0;
			}
			benchResults[i].timePerc = (benchPeriods[i].timeSum * 100) / timeDiff;
			benchResults[i].numCalls = benchPeriods[i].numCalls / benchFrame;
		}

		sortBenchResults();
	}

	void updateBench()
	{
		static char numStr[8];
		int i;

		if (profilerRunning) {
			if (getTicks() - profileTicksStart > profileTicksDuration) {
				copyBenchResults();
				clearBenchPeriods();
				profileTicksStart = getTicks();
				benchFrame = 0;
			}
			++benchFrame;

			for (i=0; i<=maxBenchIndex; ++i) {
				const int py = i * 12;
				const Word col = MakeRGB15(8,12,20);

				if (benchResultsSorted[i]) {
					const int perc = benchResultsSorted[i]->timePerc;
					if (perc != 0)
						DrawARect(0,py-1, (perc * 320) / 100, 10, col);
				}
			}
			FlushCCBs();

			for (i=0; i<=maxBenchIndex; ++i) {
				const int py = i * 12;

				if (benchResultsSorted[i]) {
					drawNumber(0, py, benchResultsSorted[i]->numCalls);
					if (displayPercentage) {
						sprintf(numStr, "%d%%", benchResultsSorted[i]->timePerc);
						drawText(32, py, numStr);
					} else {
						const int ms = benchResultsSorted[i]->timeTics / 1000;
						const int msFrac = (benchResultsSorted[i]->timeTics - (ms * 1000)) / 10;
						sprintf(numStr, "%d.%02d", ms, msFrac);
						drawText(32, py, numStr);
					}
					if (benchResultsSorted[i]->name != NULL) {
							drawText(80, py, benchResultsSorted[i]->name);
					}
				}
			}
		}
	}
#endif
