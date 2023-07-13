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

#endif // _TIME_H