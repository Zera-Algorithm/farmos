#include <proc/cpu.h>
#include <proc/sched.h>
#include <proc/sleep.h>

void sleep(void *chan, mutex_t *mtx, const char *msg) {
	thread_t *td = cpu_this()->cpu_running;
	// 先获取进程锁，再释放传入的另一个锁，保证另一个锁释放后当前进程的状态不会被修改
	mtx_lock(&td->td_lock);
	mtx_unlock(mtx);
	// 自己将自己加入睡眠队列（被唤醒时由唤醒方将本进程从睡眠队列移动到调度队列）
	tdq_critical_enter(&thread_sleepq);
	TAILQ_INSERT_TAIL(&thread_sleepq.tq_head, td, td_sleepq);
	tdq_critical_exit(&thread_sleepq);

	// 保存睡眠信息
	td->td_wchan = (ptr_t)chan;
	td->td_status = SLEEPING;
	td->td_wmesg = msg;

	// 释放进程锁，进入睡眠
	schedule();

	// 此时已经被唤醒，进程状态为 RUNNING，清空睡眠信息
	td->td_wchan = 0;
	td->td_wmesg = NULL;

	// 释放进程锁，重新获取传入的另一个锁
	mtx_unlock(&td->td_lock);
	mtx_lock(mtx);
}

void wakeup(void *chan) {
	thread_t *td = NULL;
	threadq_t readyq;
	TAILQ_INIT(&readyq.tq_head);
	// 将睡眠队列中所有等待 chan 的进程唤醒
	tdq_critical_enter(&thread_sleepq);
	TAILQ_FOREACH (td, &thread_sleepq.tq_head, td_sleepq) {
		mtx_lock(&td->td_lock);
		if (td->td_status == SLEEPING && td->td_wchan == (ptr_t)chan) {
			td->td_status = RUNNABLE;
			TAILQ_REMOVE(&thread_sleepq.tq_head, td, td_sleepq);
			TAILQ_INSERT_TAIL(&readyq.tq_head, td, td_runq);
		}
		mtx_unlock(&td->td_lock);
	}
	tdq_critical_exit(&thread_sleepq);

	// 将唤醒的进程加入就绪队列
	tdq_critical_enter(&thread_runq);
	TAILQ_CONCAT(&thread_runq.tq_head, &readyq.tq_head, td_runq);
	tdq_critical_exit(&thread_runq);
}
