#ifndef _NANOSLEEP_H
#define _NANOSLEEP_H
#include <lib/queue.h>
#include <types.h>

typedef struct thread thread_t;

typedef struct nanosleep_data {
	u32 valid;
	u64 start_time;
	u64 clocks;
	u64 end_time;
	LIST_ENTRY(nanosleep_data) nano_link;
} nanosleep_t;

LIST_HEAD(nanosleep_list, nanosleep_data);

void nanosleep_proc(u64 clocks);
void nanosleep_check();

#endif
