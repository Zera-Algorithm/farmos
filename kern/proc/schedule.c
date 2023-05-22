#include <lib/string.h>
#include <mm/memlayout.h>
#include <mm/memory.h>
#include <param.h>
#include <proc/proc.h>
#include <riscv.h>
#include <trap/trap.h>
#include <types.h>

/**
 * @brief 需要为每个cpu都定义一个调度队列，
 *        在procCreate时将新进程按照一定的规则分配到cpu上
 *
 */

/**
 * @brief 调度函数，调度下一个进程到当前cpu上
 * @param yield 是否需要当前进程让出cpu
 */
void schedule(u64 yield) {
	static int count = 1;
	struct Proc *proc = myProc();
	log("schedule: count = %d\n", count);
	int cpu = cpuid();

	if (yield || count == 0 || proc == NULL || !procCanRun(proc)) {
		if (proc != NULL && procCanRun(proc)) {
			TAILQ_REMOVE(&procSchedQueue[cpu], proc, procSchedLink[cpu]);
			TAILQ_INSERT_TAIL(&procSchedQueue[cpu], proc, procSchedLink[cpu]);
		}

		proc = TAILQ_FIRST(&procSchedQueue[cpu]);
		if (proc == NULL) {
			panic("schedule: no runnable envs\n");
		}
		count = proc->priority;
	}
	count -= 1;
	procRun(proc);
}
