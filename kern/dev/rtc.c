#include <dev/rtc.h>
#include <mm/memlayout.h>
#include <types.h>

// reference:
// https://github.com/RMerl/asuswrt-merlin.ng/blob/f4e3563d403dff0ccf610f20504f4a3a0580f5e1/release/src-rt-5.04axhnd.675x/kernel/linux-4.19/drivers/rtc/rtc-goldfish.c#L1
// RTC: Google Goldfish

#define R(r) ((volatile uint32 *)(RTC_BASE + (r)))

/**
 * @brief 读取RTC实时钟，获取以us计的Unix时间戳（即自1970年以来的时间数）
 */
u64 rtcReadTime() {
	u64 time_high;
	u64 time_low;
	u64 time;

	// 得到的time是以ns计的时间
	time_low = *R(TIMER_TIME_LOW);
	time_high = *R(TIMER_TIME_HIGH);
	time = (time_high << 32) | time_low;

	// 转换为us
	time /= NSEC_PER_USEC;
	return time;
}
