//
// 管理进程睡眠事务
//
#include <dev/timer.h>
#include <lib/queue.h>
#include <proc/proc.h>

struct ProcSleepList procNanoSleepList = {NULL};

/**
 * @brief 使进程陷入睡眠，从当前CPU的运行队列中移除，放入到睡眠队列中。但不管调度相关的事情
 */
void sleepProc(struct Proc *proc, u64 clocks) {
	proc->procTime.procSleepBegin = getTime();
	proc->procTime.procSleepClocks = clocks;

	u64 cpu = cpuid();
	TAILQ_REMOVE(&procSchedQueue[cpu], proc, procSchedLink[cpu]);
	LIST_INSERT_HEAD(&procNanoSleepList, proc, procSleepLink);
}

/**
 * @brief 在每次时钟中断时调用。检查当前是否有可唤醒的进程
 */
void wakeupProc() {
	struct Proc *proc;
	u64 curTime = getTime();
	u64 cpu = cpuid();
	int pick = 0;

	do {
		pick = 0;
		LIST_FOREACH (proc, &procNanoSleepList, procSleepLink) {
			// loga("proc's pid = 0x%08lx\n", proc->pid);
			// 该进程睡眠时间已超过设定时间
			if (proc->procTime.procSleepBegin + proc->procTime.procSleepClocks <=
			    curTime) {
				pick = 1;
				// 将此进程从睡眠队列中移除，加入到调度队列
				LIST_REMOVE(proc, procSleepLink);
				TAILQ_INSERT_HEAD(&procSchedQueue[cpu], proc, procSchedLink[cpu]);
				break;
			}
		}
	} while (pick == 1);
}
