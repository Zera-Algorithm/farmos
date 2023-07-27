#include <dev/timer.h>
#include <futex/futex.h>
#include <lib/log.h>
#include <proc/thread.h>
#include <proc/tsleep.h>

futexevent_t futexevents[FUTEXEVENTS_MAX];

futexeventq_t fe_freeq;
futexeventq_t fe_usedq;

// 初始化 futex 事件
void futexevent_init() {
	// 初始化互斥锁
	mtx_init(&fe_freeq.ftxq_lock, "futex_freeq", false, MTX_SPIN);
	mtx_init(&fe_usedq.ftxq_lock, "futex_usedq", false, MTX_SPIN);

	// 初始化队列
	TAILQ_INIT(&fe_freeq.ftxq_head);
	TAILQ_INIT(&fe_usedq.ftxq_head);

	// 初始化 futex 事件
	for (int i = FUTEXEVENTS_MAX - 1; i >= 0; i--) {
		futexevent_t *fe = &futexevents[i];
		fe->ftx_upaddr = 0;
		fe->ftx_waiterpid = 0;
		TAILQ_INSERT_HEAD(&fe_freeq.ftxq_head, fe, ftx_freeq);
	}
}

// 从 futex 事件队列中获取一个 futex 事件
futexevent_t *futexevent_alloc(u64 uaddr, u64 pid, u64 timeout) {
	futexevent_t *fe = NULL;
	feq_critical_enter(&fe_freeq);
	assert(!TAILQ_EMPTY(&fe_freeq.ftxq_head));
	fe = TAILQ_FIRST(&fe_freeq.ftxq_head);
	TAILQ_REMOVE(&fe_freeq.ftxq_head, fe, ftx_freeq);
	feq_critical_exit(&fe_freeq);

	fe->ftx_upaddr = uaddr;
	fe->ftx_waiterpid = pid;
	fe->ftx_waketime = timeout == 0 ? 0 : timeout + getUSecs();
	warn("futexevent_alloc: until %d\n", timeout);

	feq_critical_enter(&fe_usedq);
	TAILQ_INSERT_TAIL(&fe_usedq.ftxq_head, fe, ftx_link);
	return fe;
}

/**
 * @note 使用时需持有使用队列的锁
 */
void futexevent_free_and_wake(futexevent_t *fe) {
	fe->ftx_upaddr = 0;
	fe->ftx_waiterpid = 0;
	fe->ftx_waketime = 0;

	assert(mtx_hold(&fe_usedq.ftxq_lock));
	TAILQ_REMOVE(&fe_usedq.ftxq_head, fe, ftx_link);

	feq_critical_enter(&fe_freeq);
	TAILQ_INSERT_HEAD(&fe_freeq.ftxq_head, fe, ftx_freeq);
	feq_critical_exit(&fe_freeq);

	twakeup(fe);
}
