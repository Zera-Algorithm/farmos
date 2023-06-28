#ifndef _THREAD_H_
#define _THREAD_H_

#include <lib/queue.h>
#include <lock/mutex.h>
#include <proc/context.h>
#include <trap/trapframe.h>
#include <types.h>

#define MAX_PROC_NAME_LEN 16
#define MAX_FD_COUNT 256
#ifndef NPROC
#define NPROC 1024
#endif

typedef enum thread_state { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE } thread_state_t;

typedef struct thread {
	context_t td_context; // 内核上下文作为第一个成员（由 td_lock 保护）
	ptr_t td_kstack; // 内核栈所在页的首地址（已初始化，由 td_lock 保护）
	trapframe_t *td_trapframe; // 用户态上下文（由 td_lock 保护）

	mutex_t td_lock;	  // 线程锁
	thread_state_t td_status; // 线程状态（由 td_lock 保护）

	u64 td_tid; // 线程id（由 td_lock 保护）
	// u64 td_mutex_depth; // 线程锁深度（必须通过 cpu_this 调用，由关闭中断保护）
	// u64 td_mutex_saved_sstatus; // 线程锁值（必须通过 cpu_this 调用，由关闭中断保护）

	ptr_t td_wchan;	      // 线程等待的 chan（由 td_lock 保护）
	const char *td_wmesg; // 线程等待的原因（由 td_lock 保护）

	// should in proc
	pte_t *td_pt; // 线程用户态页表
	u64 td_pid;   // 线程所属进程
	u64 td_ppid;  // 线程所属进程的父进程
	int fdList[MAX_FD_COUNT];
	struct PipeWait {
		int i;
		struct Pipe *p;
		int kernFd;
		int count;
		u64 buf;
		int fd;
	} pipeWait;
	struct Dirent *cwd;	      // Current directory
	char name[MAX_PROC_NAME_LEN]; // Process name (debugging)
	// should in proc end

	TAILQ_ENTRY(thread) td_runq;   // 运行队列
	TAILQ_ENTRY(thread) td_freeq;  // 运行队列
	TAILQ_ENTRY(thread) td_sleepq; // 睡眠队列
} thread_t;

extern thread_t threads[NPROC];

void td_switch(thread_t *oldtd, register_t param); // switch.S
void td_initentry(thread_t *inittd);		   // switch.S

thread_t *td_alloc();

void thread_init();

// 线程队列
typedef struct threadq {
	TAILQ_HEAD(, thread) tq_head;
	mutex_t tq_lock;
} threadq_t;

extern threadq_t thread_runq;
extern threadq_t thread_freeq;
extern threadq_t thread_sleepq;

#define tdq_critical_enter(tdq) mtx_lock(&(tdq)->tq_lock)
#define tdq_critical_exit(tdq) mtx_unlock(&(tdq)->tq_lock)

// deprecated
// elf mapper
int loadCode(thread_t *td, const void *binary, size_t size, u64 *maxva);
// old pt init
void td_initupt(thread_t *proc);

#endif // _THREAD_H_