#ifndef _TIME_H
#define _TIME_H

#include <types.h>

typedef struct timeval {
	time_t tv_sec;	     /* seconds */
	suseconds_t tv_usec; /* microseconds */
} timeval_t;

typedef struct timezone {
	int tz_minuteswest; /* minutes west of Greenwich */
	int tz_dsttime;	    /* type of DST correction */
} timezone_t;

typedef struct timespec {
	time_t tv_sec; /* seconds */
	long tv_nsec;  /* nanoseconds */
} timespec_t;

// clock_gettime获取时间的类型
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7
#define CLOCK_REALTIME_ALARM 8
#define CLOCK_BOOTTIME_ALARM 9

#include <dev/timer.h>

// 工具

#define TV_USEC(tv) ((tv).tv_sec * USEC_PER_SEC + (tv).tv_usec)
#define TS_USEC(ts) ((ts).tv_sec * USEC_PER_SEC + (ts).tv_nsec / (NSEC_PER_SEC / USEC_PER_SEC))

#endif // _TIME_H
