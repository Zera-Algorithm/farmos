#ifndef _RTC_H
#define _RTC_H
#include <types.h>

// RTC的寄存器定义
// reference:
// https://github.com/RMerl/asuswrt-merlin.ng/blob/f4e3563d403dff0ccf610f20504f4a3a0580f5e1/release/src-rt-5.04axhnd.675x/kernel/linux-4.19/drivers/rtc/rtc-goldfish.c#L1
#define TIMER_TIME_LOW 0x00   /* get low bits of current time  */
			      /*   and update TIMER_TIME_HIGH  */
#define TIMER_TIME_HIGH 0x04  /* get high bits of time at last */
			      /*   TIMER_TIME_LOW read         */
#define TIMER_ALARM_LOW 0x08  /* set low bits of alarm and     */
			      /*   activate it                 */
#define TIMER_ALARM_HIGH 0x0c /* set high bits of next alarm   */
#define TIMER_IRQ_ENABLED 0x10
#define TIMER_CLEAR_ALARM 0x14
#define TIMER_ALARM_STATUS 0x18
#define TIMER_CLEAR_INTERRUPT 0x1c

#define NSEC_PER_USEC 1000000ul

u64 rtcReadTime();
#endif
