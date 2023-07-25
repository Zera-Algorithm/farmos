#ifndef _FUTEX_H
#define _FUTEX_H

#include <lib/queue.h>
#include <lock/mutex.h>
#include <types.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_REQUEUE 3
#define FUTEX_PRIVATE_FLAG 128

typedef struct thread thread_t;

// 内核结构体
typedef struct futexevent {
	u64 ftx_upaddr;			   // 锁的物理地址
	u64 ftx_waiterpid;		   // 等待者的 pid
	u64 ftx_waketime;		   // 唤醒时间(us)
	TAILQ_ENTRY(futexevent) ftx_freeq; // 链接到空闲队列
	TAILQ_ENTRY(futexevent) ftx_link;  // 链接到使用队列
} futexevent_t;

typedef struct futexeventq {
	mutex_t ftxq_lock;		    // 互斥锁
	TAILQ_HEAD(, futexevent) ftxq_head; // 队列
} futexeventq_t;

// 快速用户空间互斥锁接口
void futexevent_init();

err_t futex_wait(u64 uaddr, u64 val, u64 utimeout);
err_t futex_wake(u64 uaddr, u64 wakecnt);
err_t futex_requeue(u64 srcuaddr, u64 dstuaddr, u64 wakecnt, u64 maxwaiter);

futexevent_t *futexevent_alloc(u64 uaddr, u64 pid, u64 wake);
void futexevent_free_and_wake(futexevent_t *fe);
void futexevent_check();

extern futexeventq_t fe_usedq;

#define FUTEXEVENTS_MAX 1024

#define feq_critical_enter(feq) mtx_lock(&(feq)->ftxq_lock)
#define feq_critical_exit(feq) mtx_unlock(&(feq)->ftxq_lock)

#endif // _FUTEX_H