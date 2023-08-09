#include "dev/dtb.h"
#include "dev/sbi.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include <dev/timer.h>
#include <sys/time.h>

/**
 * @brief 打开全局中断，设置核内时钟下一Tick的时间，以初始化时钟
 */
void timerInit() {
	SBI_SET_TIMER(time_mono_clock() + INTERVAL);
}

/**
 * @brief 用于在发生时钟中断时设置下一个Tick的时间
 */
static void timer_set_next_tick() {
	SBI_SET_TIMER(time_mono_clock() + INTERVAL);
}

/**
 * @brief 处理时钟中断：设置下一个tick的时间，并尝试唤醒nanosleep
 */
void handler_timer_int() {
	timer_set_next_tick();
}

// offset以时钟周期数为单位，实际为1000s
#define RTC_CLOCK_CNT_OFFSET (1000ull * CLOCK_PER_SEC)
#define CLOCK_TO_NSEC(clk) ((clk) * NSEC_PER_CLOCK)
#define CLOCK_TO_USEC(clk) ((clk) / CLOCK_PER_USEC)
#define CLOCK_TO_SEC(clk) ((clk) / CLOCK_PER_SEC)

time_t time_mono_clock() {
	uint64 n;
	asm volatile("rdtime %0" : "=r"(n));
	return n;
}

time_t time_rtc_clock() {
	return time_mono_clock() + RTC_CLOCK_CNT_OFFSET;
}

time_t time_mono_ns() {
	return CLOCK_TO_NSEC(time_mono_clock()) ;
}

time_t time_mono_us() {
	return CLOCK_TO_USEC(time_mono_clock());
}

time_t time_rtc_ns() {
	return CLOCK_TO_NSEC(time_rtc_clock());
}

time_t time_rtc_us() {
	return CLOCK_TO_USEC(time_rtc_clock());
}

timeval_t time_rtc_tv() {
	time_t clk = time_rtc_clock();
	timeval_t tv;
	tv.tv_sec = CLOCK_TO_SEC(clk);
	tv.tv_usec = CLOCK_TO_USEC(clk) % USEC_PER_SEC;
	return tv;
}

timeval_t time_mono_tv() {
	time_t clk = time_mono_clock();
	timeval_t tv;
	tv.tv_sec = CLOCK_TO_SEC(clk);
	tv.tv_usec = CLOCK_TO_USEC(clk) % USEC_PER_SEC;
	return tv;
}

timespec_t time_rtc_ts() {
	time_t clk = time_rtc_clock();
	timespec_t ts;
	ts.tv_sec = CLOCK_TO_SEC(clk);
	ts.tv_nsec = CLOCK_TO_NSEC(clk) % NSEC_PER_SEC;
	return ts;
}

timespec_t time_mono_ts() {
	time_t clk = time_mono_clock();
	timespec_t ts;
	ts.tv_sec = CLOCK_TO_SEC(clk);
	ts.tv_nsec = CLOCK_TO_NSEC(clk) % NSEC_PER_SEC;
	return ts;
}
