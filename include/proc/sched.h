#ifndef _SCHED_H_
#define _SCHED_H_

#include <lib/queue.h>
#include <proc/thread.h>
#include <types.h>
void sched_init();
void schedule();
void yield();
thread_t *sched_switch(thread_t *old, register_t param);

#endif // _SCHED_H_