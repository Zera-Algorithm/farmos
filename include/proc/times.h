#ifndef _TIMES_H
#define _TIMES_H

#include <types.h>

typedef struct times {
	clock_t tms_utime;  /* user time */
	clock_t tms_stime;  /* system time */
	clock_t tms_cutime; /* user time of children */
	clock_t tms_cstime; /* system time of children */

	clock_t tms_ustart; /* U state start time */
} times_t;

#endif /* _TIMES_H */
