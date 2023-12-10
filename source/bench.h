#ifndef BENCH_H
#define BENCH_H

//#define PROFILE_ON

#ifdef PROFILE_ON
	#define MAX_BENCH_PERIODS 25

	typedef struct uTime
	{
		uint32 seconds;
		uint32 useconds;
	}uTime;

	typedef struct BenchPeriod
	{
		char *name;
		uTime timeStart;
		uint32 timeSum;
		int numCalls;
	}BenchPeriod;

	typedef struct BenchResult
	{
		char *name;
		uint32 timeTics;
		uint32 timePerc;
		int numCalls;
	}BenchResult;
#endif


extern int nframe;

void initTimer(void);
int getTicks(void);
void printFPS(int px, int py);

#ifdef PROFILE_ON
	void clearBenchPeriods(void);
	void startBenchPeriod(int index, char *name);
	void endBenchPeriod(int index);
	void updateBench(void);
	void startProfiling(int seconds);
	void stopProfiling(void);
#endif


#endif
