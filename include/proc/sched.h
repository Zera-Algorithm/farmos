#ifndef _SCHED_H_
#define _SCHED_H_

#include <proc/thread.h>
#include <types.h>
void schedule();
void yield();

void sched_init() __attribute__((noreturn));
context_t *sched_switch(context_t *old_ctx, register_t param);

extern clock_t ticks;

#endif // _SCHED_H_