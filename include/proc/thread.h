#ifndef _THREAD_H_
#define _THREAD_H_

#include <fs/thread_fs.h>
#include <lib/queue.h>
#include <lock/mutex.h>
#include <proc/context.h>
#include <proc/times.h>
#include <trap/trapframe.h>
#include <types.h>

#define MAX_PROC_NAME_LEN 32
#define MAX_FD_COUNT 256
#ifndef NPROC
#define NPROC 1024
#endif

// init进程的tid
#define TID_INIT (0 | (1 * NPROC))

typedef enum thread_state { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE } thread_state_t;

typedef struct thread {
	// 线程核心属性
	context_t td_context; // 内核上下文作为第一个成员（由 td_lock 保护）
	ptr_t td_kstack; // 内核栈所在页的首地址（已被初始化，由 td_lock 保护）
	trapframe_t *td_trapframe; // 用户态上下文（由 td_lock 保护）

	mutex_t td_lock;		 // 线程锁（已被初始化）
	thread_state_t td_status;	 // 线程状态（由 td_lock 保护）
	char td_name[MAX_PROC_NAME_LEN]; // 线程名 todo fork时溢出

	u64 td_tid;	 // 线程id（由 td_lock 保护）
	u64 td_exitcode; // 线程退出码（由 td_lock 保护）

	// 睡眠相关
	ptr_t td_wchan;	      // 线程等待的 chan（由 td_lock 保护）
	const char *td_wmesg; // 线程等待的原因（由 td_lock 保护）

	// should in proc
	times_t td_times; // 线程运行时间，只有自己写入，父进程 wait 时读僵尸子进程，不用保护
	pte_t *td_pt; // 线程用户态页表
	u64 td_pid;   // 线程所属进程(todo)

	ptr_t td_brk; // 进程堆顶
	thread_fs_t td_fs_struct;

	// should in proc end

	// 线程队列相关
	TAILQ_ENTRY(thread) td_runq;   // 运行队列
	TAILQ_ENTRY(thread) td_freeq;  // 自由队列
	TAILQ_ENTRY(thread) td_sleepq; // 睡眠队列

	struct thread *td_parent;	  // 父线程（只会由父进程修改，不加锁）
	LIST_HEAD(, thread) td_childlist; // 子线程
	LIST_ENTRY(thread) td_childentry; // 用于将当前线程加入父线程的子线程链表
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

// 线程初始化
void td_initupt(thread_t *td);
void td_initustack(thread_t *td, u64 ustack);
void td_setustack(thread_t *td, u64 argc, char **argv);
void td_initucode(thread_t *td, const void *bin, size_t size);

// 线程回收
void td_destroy() __attribute__((noreturn));
void td_free(thread_t *td);
void td_recycleupt(thread_t *td);

// 线程创建
void td_create(const char *name, const void *bin, size_t size);
u64 td_fork(thread_t *td, u64 childsp);

#define TD_CREATE(program, name)                                                                   \
	({                                                                                         \
		extern char binary_##program[];                                                    \
		extern int binary_##program##_size;                                                \
		td_create(name, binary_##program, binary_##program##_size);                        \
	})

#endif // _THREAD_H_
