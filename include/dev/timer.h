#ifndef _TIMER_H_
#define _TIMER_H_

#include <types.h>
#include <sys/time.h>
#include <feature.h>

#define USEC_PER_SEC 1000000ul
#define NSEC_PER_SEC 1000000000ul

// 每us的时钟数
#define CLOCK_PER_SEC FEATURE_TIMER_FREQ
#define CLOCK_PER_USEC (CLOCK_PER_SEC / USEC_PER_SEC)
#define CLOCK_TO_USEC(clk) ((clk) / CLOCK_PER_USEC)
#define NSEC_PER_CLOCK (NSEC_PER_SEC / CLOCK_PER_SEC)

// 中断时间间隔为0.05s(20Hz, 20times/s)
// 这个时间间隔以周期数计算
#define INTERVAL (FEATURE_TIMER_FREQ / 20)

#define RTC_OFF (10000000000000ul)

void timerInit();
void handler_timer_int();


// Modified
time_t time_mono_clock();
time_t time_rtc_clock();
time_t time_mono_ns();
time_t time_mono_us();
time_t time_rtc_ns();
time_t time_rtc_us();
timeval_t time_rtc_tv();
timeval_t time_mono_tv();
timespec_t time_rtc_ts();
timespec_t time_mono_ts();


#endif
