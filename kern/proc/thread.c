#include <lib/log.h>
#include <lib/string.h>
#include <lock/mutex.h>
#include <mm/memlayout.h>
#include <proc/cpu.h>
#include <proc/sched.h>
#include <proc/sleep.h>
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
		tdq_critical_exit(&thread_freeq);
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
	td->td_context.ctx_sp = td->td_kstack + TD_KSTACK_SIZE;

	return td;
}

/**
 * @brief 基于已初始化的内核线程，初始化用户线程，加载代码段，并返回（不加入调度队列）。
 * @param td 具有内核栈，页表中已初始化 Trampoline/Trapframe 的线程，当前持有锁。
 */
static void td_uvminit(thread_t *td, const char *name, const void *bin, size_t size) {
	// 初始化用户地址栈空间（若已有栈空间则原先的栈会被解引用）
	td_initustack(td, TD_USTACK);

	// 初始化用户代码段
	td_initucode(td, bin, size);

	// 设置进程信息
	assert(strlen(name) < sizeof(td->td_name) - 1);
	strncpy(td->td_name, name, sizeof(td->td_name));
}

/**
 * @brief 创建初始化线程，必须在开始调度之前调用。
 */
void td_create(const char *name, const void *bin, size_t size) {
	// 初始化用户线程地址空间
	thread_t *td = td_alloc();
	td_uvminit(td, name, bin, size);

	// 初始化进程的文件系统结构体
	init_thread_fs(&td->td_fs_struct);

	// 设置进程运行状态
	td->td_status = RUNNABLE;

	// 初始化参数
	td_setustack(td, 0, NULL);

	// 加入调度队列
	tdq_critical_enter(&thread_runq);
	TAILQ_INSERT_TAIL(&thread_runq.tq_head, td, td_runq);
	mtx_unlock(&td->td_lock);
	tdq_critical_exit(&thread_runq);
}

/**
 * @brief 回收传入僵尸进程的进程控制块（其余资源已被回收）。
 * @note 进程自己调用 exit 变为僵尸进程或被 kill 后变成僵尸进程。
 * @pre 必须持有传入线程的锁
 */
void td_free(thread_t *td) {
	assert(td->td_status == ZOMBIE);
	assert(mtx_hold(&td->td_lock));

	log(LEVEL_GLOBAL, "td %s is freed\n", td->td_name);

	// 将线程字段重置
	td->td_status = UNUSED;
	td->td_name[0] = '\0';
	td->td_tid = 0;
	td->td_wchan = 0;
	td->td_wmesg = NULL;
	td->td_parent = NULL;

	// 进程字段（TODO）
	td->td_pid = 0;

	// 将线程加入空闲线程队列
	tdq_critical_enter(&thread_freeq);
	TAILQ_INSERT_TAIL(&thread_freeq.tq_head, td, td_freeq);
	tdq_critical_exit(&thread_freeq);
}

/**
 * @brief 回收线程资源，将其变为僵尸进程。
 * @note 仅在线程本身退出时调用。
 */
static void td_recycle(thread_t *td) {
	// 回收全部页表
	td_recycleupt(td);

	// 回收进程描述符 todo
	td->td_status = ZOMBIE;

	// 回收进程的fs资源
	recycle_thread_fs(&td->td_fs_struct);
}

/**
 * @brief 当前进程主动调用 exit 或被 kill 后，回收进程资源。
 */
void td_destroy() {
	thread_t *td = cpu_this()->cpu_running;

	// 回收进程资源
	mtx_lock(&td->td_lock);

	log(LEVEL_GLOBAL, "destroy thread %s\n", td->td_name);

	td_recycle(td);
	mtx_unlock(&td->td_lock);

	// 拿等待锁，防止其它线程在此期间调用 wait
	mtx_lock(&wait_lock);

	// 处理子进程资源
	thread_t *child;
	// 自己的子线程链表为私有，对其它线程不可见，不加锁，但修改子进程状态（td_free）时需要加锁
	LIST_UNTIL_EMPTY(child, &td->td_childlist) {
		mtx_lock(&child->td_lock);
		LIST_REMOVE(child, td_childentry);
		// 根据子进程状态处理
		if (child->td_status == ZOMBIE) {
			td_free(child);
		} else {
			warn("haven't implement init, child %d is still alive", child->td_tid);
			child->td_parent = 0;
			// child->td_parent = TID_INIT;
			// todo: insert to init's childlist and wake up init
		}
		mtx_unlock(&child->td_lock);
	}

	// 通知父进程（父进程 wait 时等待的是父线程(self)的指针）
	wakeup(td->td_parent);

	// 拿锁，调度新进程
	mtx_lock(&td->td_lock);
	mtx_unlock(&wait_lock);
	schedule();
	error("td_destroy: should not reach here");
}
