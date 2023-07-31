#include <futex/futex.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <mm/memlayout.h>
#include <proc/cpu.h>
#include <proc/sched.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <trap/trap.h>

mutex_t td_tid_lock;

static u64 tid_alloc(thread_t *td) {
	static u64 cnt = 1; // todo tid lock
	mtx_lock(&td_tid_lock);
	cnt += 1;
	u64 new_tid = TID_GENERATE(cnt, td - threads);
	mtx_unlock(&td_tid_lock);
	return new_tid;
}

/**
 * @brief 分配并初始化一个内核线程。
 * @post 返回的线程已分配好内核栈、用户态页表、被调度后从 utrap_return 开始执行。
 * @note 返回的线程持有锁，未初始化父进程。
 */
thread_t *td_alloc() {
	// 从空闲线程队列中取出一个线程
	tdq_critical_enter(&thread_freeq);
	if (TAILQ_EMPTY(&thread_freeq.tq_head)) {
		tdq_critical_exit(&thread_freeq);
		error("no free thread");
	}

	thread_t *td = TAILQ_FIRST(&thread_freeq.tq_head);
	TAILQ_REMOVE(&thread_freeq.tq_head, td, td_freeq);
	mtx_lock(&td->td_lock);
	tdq_critical_exit(&thread_freeq);
	// 已从空闲线程队列中取出线程，需要初始化线程的各个字段

	// 初始化线程字段
	td->td_tid = tid_alloc(td);
	td->td_status = USED;
	memset(&td->td_startzero, 0, rangeof(thread_t, td_startzero, td_endzero));

	// 初始化线程内核现场
	td->td_context.ctx_ra = (ptr_t)utrap_firstsched;
	td->td_context.ctx_sp = td->td_kstack + TD_KSTACK_SIZE;

	return td;
}

/**
 * @brief 回收传入僵尸进程的进程控制块（其余资源已被回收）。
 * @note 进程自己调用 exit 变为僵尸进程或被 kill 后变成僵尸进程。
 * @pre 必须持有传入线程的锁
 */
static void td_free(thread_t *td) {
	// 将线程字段重置
	td->td_proc = NULL;
	td->td_tid = 0;
	td->td_status = UNUSED;

	// 释放进程信号
	sigevent_freetd(td);

	// 将线程加入空闲线程队列
	tdq_critical_enter(&thread_freeq);
	TAILQ_INSERT_HEAD(&thread_freeq.tq_head, td, td_freeq);
	tdq_critical_exit(&thread_freeq);
}

/**
 * @brief 当前线程主动调用 exit 或被 tkill 后，回收线程资源。
 */
void td_destroy(err_t exitcode) {
	thread_t *td = cpu_this()->cpu_running;

	if (td->td_ctid) {
		mtx_lock(&td->td_lock);
		int val = 0;
		warn("td_destroy: td_ctid not null, copy 0 to it to notice other\n");
		copyOut(td->td_ctid, (void *)&val, sizeof(u32));
		warn("called futex_wake(%p, %d)\n", td->td_ctid, 1);
		futex_wake(td->td_ctid, 1); // 唤醒仍在等待的其他join线程
		mtx_unlock(&td->td_lock);
	}

	asm volatile("nop");
	log(LEVEL_GLOBAL, "destroy thread %s\n", td->td_name);

	// todo 线程残留的信号和futex
	mtx_lock(&wait_lock);
	// 将线程从进程链表中移除
	proc_lock(td->td_proc);
	TAILQ_REMOVE(&td->td_proc->p_threads, td, td_plist);
	if (TAILQ_EMPTY(&td->td_proc->p_threads)) {
		// 触发进程的摧毁
		proc_destroy(td->td_proc, exitcode);
	}
	proc_unlock(td->td_proc);
	mtx_unlock(&wait_lock);

	// 此时线程转为悬垂线程（不归属于任何进程），回收线程资源
	mtx_lock(&td->td_lock);

	td_free(td);
	// 在此之后会访问 context, status, lock 等字段，不需要进程参与
	schedule();
	error("td_destroy: should not reach here");
}
