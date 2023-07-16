#ifndef _PROC_H_
#define _PROC_H_

#include <lib/queue.h>

typedef struct thread thread_t;

typedef struct proc {

	times_t p_times; // 线程运行时间，只有自己写入，父进程 wait 时读僵尸子进程，不用保护
	pte_t *p_pt; // 线程用户态页表
	u64 p_pid;   // 线程所属进程(todo)

	ptr_t p_brk; // 进程堆顶
	thread_fs_t p_fs_struct;

	// 进程队列相关
	struct proc *p_parent; // 父线程（只会由父进程修改，不加锁）

	TAILQ_ENTRY(proc) p_freeq;     // 自由队列链接
	TAILQ_HEAD(, thread) p_tdq;    // 子进程队列
	LIST_HEAD(, proc) p_childlist; // 子进程列表
	LIST_ENTRY(proc) p_childentry; // 子进程列表链接
} proc_t;

#endif /* !_PROC_H_ */