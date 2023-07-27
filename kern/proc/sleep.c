#include <lib/log.h>
#include <proc/cpu.h>
#include <proc/sched.h>
#include <proc/sleep.h>

// 当需要调试某个睡眠的线程时，设置其睡眠原因为 # 开头的字符串
#define sleep_debug(...)                                                                                               \
	do {                                                                                                           \
		if (td->td_wmesg && td->td_wmesg[0] == '#') {                                                          \
			log(SLEEP_MODULE, __VA_ARGS__);                                                                \
		}                                                                                                      \
	} while (0)

void sleep(void *chan, mutex_t *mtx, const char *msg) {
	thread_t *td = cpu_this()->cpu_running;
	// 先获取睡眠队列锁，再释放传入的另一个锁，保证不会错过唤醒
	tdq_critical_enter(&thread_sleepq);
	mtx_unlock(mtx);

	assert(td->td_status == RUNNING);

	// 保存睡眠信息
	mtx_lock(&td->td_lock);
	td->td_wchan = (ptr_t)chan;
	td->td_status = SLEEPING;
	td->td_wmesg = msg;
	sleep_debug("%s sleeping on %x(%s)\n", cpu_this()->cpu_running->td_name, chan, msg);
	// 自己将自己加入睡眠队列（被唤醒时由唤醒方将本进程从睡眠队列移动到调度队列）
	TAILQ_INSERT_HEAD(&thread_sleepq.tq_head, td, td_sleepq);
	tdq_critical_exit(&thread_sleepq);

	// 释放进程锁，进入睡眠
	schedule();

	// 此时已经被唤醒，进程状态为 RUNNING，清空睡眠信息
	td->td_wchan = 0;
	td->td_wmesg = NULL;

	// 释放进程锁，重新获取传入的另一个锁
	mtx_unlock(&td->td_lock);
	// 当前不持有任何锁
	mtx_lock(mtx);
}

void wakeup(void *chan) {
	// log(SLEEP_MODULE, "wakeup %x\n", chan);
	thread_t *td = NULL;
	threadq_t readyq;
	TAILQ_INIT(&readyq.tq_head);
	// 将睡眠队列中所有等待 chan 的进程唤醒
	tdq_critical_enter(&thread_sleepq);
	TAILQ_FOREACH (td, &thread_sleepq.tq_head, td_sleepq) {
		mtx_lock(&td->td_lock);
		// sleep_debug("check %s(wait %x, now %x)\n", td->td_name, td->td_wchan, chan);
		assert(td->td_status == SLEEPING);
		if (td->td_wchan == (ptr_t)chan) {
			sleep_debug("wakeup %s\n", td->td_name);
			td->td_status = RUNNABLE;
			TAILQ_REMOVE(&thread_sleepq.tq_head, td, td_sleepq);
			TAILQ_INSERT_TAIL(&readyq.tq_head, td, td_runq);
		}
		mtx_unlock(&td->td_lock);
	}

	// 防止丢失唤醒的进程，先获取就绪队列锁，再获取睡眠队列锁
	tdq_critical_enter(&thread_runq);
	tdq_critical_exit(&thread_sleepq);

	// 将唤醒的进程加入就绪队列
	TAILQ_CONCAT(&thread_runq.tq_head, &readyq.tq_head, td_runq);
	tdq_critical_exit(&thread_runq);
}
