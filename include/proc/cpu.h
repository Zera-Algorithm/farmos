#ifndef _CPU_H_
#define _CPU_H_

#include <param.h>
#include <types.h>

typedef struct thread thread_t;

// Per-CPU state.
typedef struct cpu {
	thread_t *cpu_running;
	u64 cpu_mutex_depth;	     // 锁深度
	u64 cpu_mutex_saved_sstatus; // 锁值
} cpu_t;

extern cpu_t cpus[NCPU];

cpu_t *cpu_this();
register_t cpu_this_id();
void cpu_idle();

#endif // _CPU_H_