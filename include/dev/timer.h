#ifndef _TIMER_H_
#define _TIMER_H_

#include <types.h>

// 每us的时钟数
#define CLOCK_PER_USEC 10

// 中断时间间隔为0.005s(200Hz)
#define INTERVAL 50000

uint64 getTime();
u64 getUSecs();
void timerInit();
void timerSetNextTick();

#endif
