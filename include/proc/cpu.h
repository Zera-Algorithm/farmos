#ifndef _CPU_H_
#define _CPU_H_

#include <param.h>
#include <types.h>

typedef struct thread thread_t;
typedef struct mutex mutex_t;

#define MAX_CPU_LOCK 128

// Per-CPU state.
typedef struct cpu {
	thread_t *cpu_running;
	u64 cpu_lk_depth;	  // 锁深度
	mutex_t *cpu_lks[MAX_CPU_LOCK];	  // 锁名
	u64 cpu_lk_saved_sstatus; // 锁值
	bool cpu_idle;		  // 是否空闲（关联运行进程队列锁）
} cpu_t;

extern cpu_t cpus[NCPU];

cpu_t *cpu_this();
register_t cpu_this_id();
void cpu_idle();
bool cpu_allidle();
void cpu_halt() __attribute__((noreturn));

#endif // _CPU_H_
