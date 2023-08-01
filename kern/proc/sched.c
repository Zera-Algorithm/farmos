#include <lib/log.h>
#include <dev/timer.h>
#include <lib/printf.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/proc.h>
#include <proc/sched.h>
#include <proc/thread.h>
#include <riscv.h>

clock_t ticks = 0; // 总时间片数（由 thread_runq.tq_lock 保护）

/**
 * @brief 进程放弃 CPU，调度器选择新线程运行
 * @note 调用和离开该函数时，需要处在临界区内，持有对应线程的锁并且中断被关闭
 */
void schedule() {
	assert(intr_get() == 0);
	assert(mtx_hold(&cpu_this()->cpu_running->td_lock));
	if (cpu_this()->cpu_lk_depth != 1) {
		asm volatile("ebreak");
		panic("schedule: cpu_lk_depth %d\n", cpu_this()->cpu_lk_depth);
	}
	if (cpu_this()->cpu_running->td_lock.mtx_depth != 1) {
		panic("schedule: td_lock.mtx_depth %d\n",
		      cpu_this()->cpu_running->td_lock.mtx_depth);
	}
	assert(cpu_this()->cpu_running->td_status != RUNNING);
	/**
	 *     从线程的视角来看，它调用了 td_switch，传入了自己的上下文和一个参数 0。
	 * 下一次它被调度运行时是从 td_switch 返回。
	 *     实际调用栈如下：
	 *     ctx_switch(switch.S) -> sched_switch -> ctx_switch(switch.S) -> schedule
	 */
	ctx_switch(&cpu_this()->cpu_running->td_context, 0);
	// 此时已经切换到新线程，且新线程已经获取锁
}

void yield() {
	thread_t *td = cpu_this()->cpu_running;
	// 此时不持有任何锁
	mtx_lock(&td->td_lock);
	td->td_status = RUNNABLE;
	schedule();
	mtx_unlock(&td->td_lock);
	// 此时不持有任何锁
}

/**
 * @brief 从可执行队列中选出一个线程，从队列中移除并加锁返回，是每次调度的核心
 */
static thread_t *sched_runnable(thread_t *old) {
	tdq_critical_enter(&thread_runq);
	// 将旧线程放回队列
	if (old != NULL) {
		// 如果旧线程仍然可运行，放回队列
		if (old->td_status == RUNNABLE) {
			TAILQ_INSERT_TAIL(&thread_runq.tq_head, old, td_runq);
		} else if (old->td_status == SLEEPING) {
			log(SLEEP_MODULE, "Thread %s(%d) sleeping on %x\n", old->td_name,
			    old->td_tid, old->td_wchan);
		}
		mtx_unlock(&old->td_lock);
	}

	while (TAILQ_EMPTY(&thread_runq.tq_head)) {
		cpu_this()->cpu_idle = true;
		// 放锁前尝试查看睡眠队列
		if (tdq_critical_try_enter(&thread_sleepq)) {
			// 运行队列为空，睡眠队列为空，关机
			if (TAILQ_EMPTY(&thread_sleepq.tq_head) && cpu_allidle()) {
				warn("No thread alive, halt\n");
				cpu_halt();
			}
			// 有进程睡眠，不关机
			tdq_critical_exit(&thread_sleepq);
		}
		tdq_critical_exit(&thread_runq);
		// 等待新线程加入队列
		// log(LEVEL_GLOBAL, "No thread runnable, idle\n");
		cpu_idle();
		tdq_critical_enter(&thread_runq);
	}
	cpu_this()->cpu_idle = false;

	// 选择新线程
	thread_t *ret = TAILQ_FIRST(&thread_runq.tq_head);
	mtx_lock(&ret->td_lock);
	TAILQ_REMOVE(&thread_runq.tq_head, ret, td_runq);

	tdq_critical_exit(&thread_runq);

	return ret;
}

void sched_init() {
	assert(intr_get() == 0);
	assert(cpu_this()->cpu_lk_depth == 0);
	cpu_t *cpu = cpu_this();

	printf("Hart %d start running.\n", cpu_this_id());

	// 选择一个线程
	thread_t *ret = sched_runnable(NULL);
	// 设置当前线程
	cpu->cpu_running = ret;
	ret->td_status = RUNNING;
	// 运行线程
	ctx_enter(&ret->td_context);
}

/**
 * @brief 仅由 switch.S 调用，已保存旧线程的上下文，本函数需要进行调度，选择接下来运行的新线程
 * @param oldtd 旧线程（已经获取锁，已经保存上下文）
 * @param param 旧线程请求切换线程时传入的参数
 * @note 进入该函数时，中断需被关闭，旧线程的锁已被获取
 */
context_t *sched_switch(context_t *old_ctx, register_t param) {
	thread_t *old = container_of(old_ctx, thread_t, td_context);
	// 释放旧线程的锁
	assert(mtx_hold(&old->td_lock));

	// 清理旧线程状态
	cpu_t *cpu = cpu_this();
	cpu->cpu_running = NULL;

	// 选择新线程
	thread_t *ret = sched_runnable(old);
	// log(LEVEL_GLOBAL, "Hart Sched %s(%d) -> %s(%d)\n", old->td_name, old->td_tid,
	// ret->td_name,
	//     ret->td_tid);

	cpu->cpu_running = ret;
	ret->td_status = RUNNING;
	return &ret->td_context;
}
