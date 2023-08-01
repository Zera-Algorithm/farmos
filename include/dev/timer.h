#ifndef _TIMER_H_
#define _TIMER_H_

#include <types.h>

#define USEC_PER_SEC 1000000ul
#define NSEC_PER_SEC 1000000000ul

// 每us的时钟数
#define CLOCK_PER_SEC 10000000ul
#define CLOCK_PER_USEC (CLOCK_PER_SEC / USEC_PER_SEC)
#define CLOCK_TO_USEC(clk) ((clk) / CLOCK_PER_USEC)
#define NSEC_PER_CLOCK 100

// 中断时间间隔为0.05s(20Hz)
#define INTERVAL 500000

#define RTC_OFF (1ul << 35)

uint64 getRealTime();
u64 getTime();
u64 getUSecs();
u64 getRealUSecs();
void timerInit();
void handler_timer_int();

#endif
