#include <lib/log.h>
#include <lib/string.h>
#include <lock/mutex.h>
#include <mm/memlayout.h>
#include <proc/thread.h>
#include <trap/trap.h>

mutex_t td_tid_lock;

u64 tid_alloc(thread_t *td) {
	static u64 cnt = 0; // todo tid lock
	mtx_lock(&td_tid_lock);
	u64 new_tid = (td - threads) | ((++cnt) * NPROC);
	mtx_unlock(&td_tid_lock);
	return new_tid;
}

/**
 * @brief 分配并初始化一个内核线程。
 * @post 返回的线程已分配好内核栈、用户态页表、被调度后从 utrap_return 开始执行。
 * @note 返回的线程持有锁，未初始化用户地址空间。
 */
thread_t *td_alloc() {
	// 从空闲线程队列中取出一个线程
	tdq_critical_enter(&thread_freeq);
	if (TAILQ_EMPTY(&thread_freeq.tq_head)) {
		mtx_unlock(&thread_freeq.tq_lock);
		error("no free thread");
	}

	thread_t *td = TAILQ_FIRST(&thread_freeq.tq_head);
	TAILQ_REMOVE(&thread_freeq.tq_head, td, td_freeq);
	mtx_lock(&td->td_lock);
	tdq_critical_exit(&thread_freeq);
	// 已从空闲线程队列中取出线程，需要初始化线程的各个字段

	// 初始化线程 ID
	td->td_tid = tid_alloc(td);

	// 初始化线程状态
	td->td_status = USED;

	// 初始化线程用户态页表（完成 Trampoline/Trapframe/）
	td_initupt(td);

	// 初始化线程内核现场
	memset(&td->td_context, 0, sizeof(td->td_context));
	td->td_context.ctx_ra = (ptr_t)utrap_firstsched;
	td->td_context.ctx_sp = td->td_kstack + KTHREAD_STACK_SIZE;

	return td;
}