#ifndef _THREAD_H_
#define _THREAD_H_

#include <lib/queue.h>
#include <lock/mutex.h>
#include <param.h>
#include <proc/context.h>
#include <proc/proc.h>
#include <signal/signal.h>
#include <trap/trapframe.h>

typedef struct proc proc_t;

#define SLEEP_DEBUG

typedef struct thread {
	mutex_t td_lock; // 线程锁（已被全局初始化）
	proc_t *td_proc; // 线程所属进程（不保护，线程初始化后只读）
	TAILQ_ENTRY(thread) td_plist;  // 所属进程的线程链表链接（进程锁保护）
	TAILQ_ENTRY(thread) td_runq;   // 运行队列链接（线程锁保护）
	TAILQ_ENTRY(thread) td_sleepq; // 睡眠队列链接（线程锁保护）
	TAILQ_ENTRY(thread) td_freeq;  // 空闲队列链接（空闲队列锁保护）
	pid_t td_tid;		       // 线程 id（不保护，线程初始化后只读）
	state_t td_status;	       // 线程状态（线程锁保护）

#define td_startzero td_name // 清零属性区域开始指针
	char td_name[MAXPATH + 1]; // 线程名（不保护，线程初始化后只读） todo fork时溢出
	ptr_t td_wchan;		   // 线程睡眠等待的地址（线程锁保护）
	const char *td_wmesg; // 线程睡眠等待的原因（线程锁保护）
	u64 td_exitcode;      // 线程退出码（线程锁保护）
	sigevent_t *td_sig;   // 线程当前正在处理的信号（线程锁保护）
	trapframe_t td_trapframe; // 用户态上下文（不保护，该指针的值线程初始化后只读）
	context_t td_context;	// 内核态上下文（不保护，只被当前线程访问）
	bool td_killed;		// 线程是否被杀死（线程锁保护）
	sigset_t td_cursigmask; // 线程正在处理的信号屏蔽字（线程锁保护）
	u64 td_ctid;		// 清空tid地址标识
#define td_startcopy td_sigmask
	sigset_t td_sigmask; // 线程信号屏蔽字（线程锁保护）
#define td_endcopy td_kstack
#define td_endzero td_kstack // 清零属性区域结束指针

	ptr_t td_kstack;	 // 内核栈所在页的首地址（已被全局初始化）
	sigeventq_t td_sigqueue; // 待处理信号队列（线程锁保护）

#ifdef SLEEP_DEBUG
	u64 td_sleep_start;    // 睡眠开始时间
	void *td_sleep_reason; // 睡眠原因
#endif

#define td_pt td_proc->p_pt
#define td_fs_struct td_proc->p_fs_struct
#define td_brk td_proc->p_brk
#define td_times td_proc->p_times
} thread_t;

// 线程队列
typedef struct threadq {
	TAILQ_HEAD(, thread) tq_head;
	mutex_t tq_lock;
} threadq_t;

extern threadq_t thread_runq;
extern threadq_t thread_freeq;
extern threadq_t thread_sleepq;
extern thread_t threads[NPROC];

thread_t *td_alloc();

void thread_init();

// 线程回收
void td_destroy(err_t exitcode) __attribute__((noreturn));

// 相关宏
#define tdq_critical_enter(tdq) mtx_lock(&(tdq)->tq_lock)
#define tdq_critical_try_enter(tdq) mtx_try_lock(&(tdq)->tq_lock)
#define tdq_critical_exit(tdq) mtx_unlock(&(tdq)->tq_lock)

#define TID_GENERATE(cnt, index) ((index) | ((cnt % 0x1000 + 0x1000) << 16))
#define TID_TO_INDEX(tid) (tid & 0xffff)

#endif // _THREAD_H_
