#include <futex/futex.h>
#include <lib/log.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/tsleep.h>
#include <proc/thread.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <dev/timer.h>

static u64 uaddr_to_pa(pte_t *pt, u64 uaddr) {
	pte_t pte = ptLookup(pt, uaddr);
	assert(pte & PTE_V);
	return pteToPa(pte) + (uaddr & (PAGE_SIZE - 1));
}

static u32 get_val(thread_t *td, u64 uaddr) {
	u32 val;
	copy_in(td->td_proc->p_pt, uaddr, &val, sizeof(u32));
	return val;
}

err_t futex_wait(u64 uaddr, u32 val, u64 utimeout) {
	thread_t *td = cpu_this()->cpu_running;
	timespec_t ts = {0, 0};
	if (utimeout) {
		copy_in(td->td_proc->p_pt, utimeout, &ts, sizeof(timespec_t));
	}
	// 获取一个等待事件（获取了使用队列锁）
	u64 upa = uaddr_to_pa(td->td_proc->p_pt, uaddr);
	futexevent_t *fe = futexevent_alloc(upa, td->td_tid, TS_USEC(ts)); // todo timeout

	// 检查值
	u32 curval = get_val(td, uaddr);
	if (val != curval) {
		warn("futex_wait: val not match: %x != %x\n", val, curval);
		futexevent_free_and_wake(fe);
		feq_critical_exit(&fe_usedq);
		return -EAGAIN;
	}


	// 睡眠（释放了使用队列锁）
	err_t r = tsleep(fe, &fe_usedq.ftxq_lock, utimeout ? "#futex_wait_timeout" : "#futex_wait", fe->ftx_waketime);
	// 释放使用队列锁并返回
	feq_critical_exit(&fe_usedq);
	if (r) {
		warn("futex_wait: tsleep failed: %d\n", r);
	}
	return r;
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
