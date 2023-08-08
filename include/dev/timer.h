#ifndef _TIMER_H_
#define _TIMER_H_

#include <types.h>
#include <sys/time.h>

#define USEC_PER_SEC 1000000ul
#define NSEC_PER_SEC 1000000000ul

// 每us的时钟数
#define CLOCK_PER_SEC 10000000ul
#define CLOCK_PER_USEC (CLOCK_PER_SEC / USEC_PER_SEC)
#define CLOCK_TO_USEC(clk) ((clk) / CLOCK_PER_USEC)
#define NSEC_PER_CLOCK 100

// 中断时间间隔为0.05s(20Hz)
// 这个时间间隔以us计算
#if ((defined QEMU_SIFIVE) || (defined VIRT))
#define INTERVAL 500000
#else
#define INTERVAL 50000
#endif

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
