#include <futex/futex.h>
#include <lib/log.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <sys/errno.h>
#include <sys/time.h>

static u64 uaddr_to_pa(pte_t *pt, u64 uaddr) {
	pte_t pte = ptLookup(pt, uaddr);
	assert(pte & PTE_V);
	return pteToPa(pte) + (uaddr & (PAGE_SIZE - 1));
}

err_t futex_wait(u64 uaddr, u64 val, u64 utimeout) {
	// todo checkval
	thread_t *td = cpu_this()->cpu_running;
	timeval_t tv = {0, 0};
	if (utimeout) {
		copy_in(td->td_proc->p_pt, utimeout, &tv, sizeof(timeval_t));
	}
	// 获取一个等待事件（获取了使用队列锁）
	u64 upa = uaddr_to_pa(td->td_proc->p_pt, uaddr);
	futexevent_t *fe = futexevent_alloc(upa, td->td_tid, TV_USEC(tv)); // todo timeout
	// 睡眠（释放了使用队列锁）
	sleep(fe, &fe_usedq.ftxq_lock, utimeout ? "futex_wait_timeout" : "futex_wait");
	// 释放使用队列锁并返回
	feq_critical_exit(&fe_usedq);
	return 0;
}

err_t futex_wake(u64 uaddr, u64 wakecnt) {
	int haswake = 0;
	u64 upa = uaddr_to_pa(cpu_this()->cpu_running->td_proc->p_pt, uaddr);

	futexevent_t *fe;
	// 遍历使用队列
	feq_critical_enter(&fe_usedq);
	TAILQ_FOREACH (fe, &fe_usedq.ftxq_head, ftx_link) {
		// 如果找到了等待者
		if (fe->ftx_upaddr == upa) {
			// 唤醒
			futexevent_free_and_wake(fe);
			haswake++;
			// 如果唤醒了足够的等待者
			if (haswake == wakecnt) {
				break;
			}
		}
	}
	// 释放使用队列锁并返回
	feq_critical_exit(&fe_usedq);
	return haswake;
}

err_t futex_requeue(u64 srcuaddr, u64 dstuaddr, u64 wakecnt, u64 maxwaiter) {
	thread_t *td = cpu_this()->cpu_running;
	u64 srcupa = uaddr_to_pa(td->td_proc->p_pt, srcuaddr);
	u64 dstupa = uaddr_to_pa(td->td_proc->p_pt, dstuaddr);

	int haswake = 0;

	futexevent_t *fe;
	// 遍历使用队列
	feq_critical_enter(&fe_usedq);
	TAILQ_FOREACH (fe, &fe_usedq.ftxq_head, ftx_link) {
		// 如果找到了等待者
		if (fe->ftx_upaddr == srcupa) {
			if (haswake < wakecnt) {
				// 唤醒
				futexevent_free_and_wake(fe);
				haswake++;
			} else {
				fe->ftx_upaddr = dstupa;
				maxwaiter--;
			}
		}
	}
	// 释放使用队列锁并返回
	feq_critical_exit(&fe_usedq);

	if (maxwaiter < 0) {
		warn("futex_requeue: maxwaiter < 0");
	}

	return haswake;
}