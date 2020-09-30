#ifndef BENCH_H
#define BENCH_H

typedef struct uTime
{
	uint32 seconds;
	uint32 useconds;
}uTime;

extern int nframe;

void initTimer(void);
int getTicks(void);
uTime getUTime(void);
int updateAndGetFPS(void);

#endif
