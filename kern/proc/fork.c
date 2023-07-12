#include <lib/string.h>
#include <mm/vmm.h>
#include <mm/vmtools.h>
#include <proc/cpu.h>
#include <proc/thread.h>

static err_t duppage(Pte *pd, u64 target_va, Pte *target_pte, void *arg) {
	pte_t *childpd = (pte_t *)arg;
	// 跳过 Trapframe/Trampoline（已在新内核线程中映射过，不需要再映射）
	if (ptLookup(childpd, target_va) == 0) {
		// 从父线程的页表中获取映射信息
		pte_t parentpte = ptLookup(pd, target_va);
		u64 perm = PTE_PERM(parentpte);
		// 如果父线程的页表项是用户可写的，则进行写时复制（没考虑 PTE_G todo）
		if ((perm & PTE_W) && (perm & PTE_U)) {
			perm = (perm & ~PTE_W) | PTE_COW;
			return ptMap(childpd, target_va, pteToPa(parentpte), perm) ||
			       ptMap(pd, target_va, pteToPa(parentpte), perm);
		} else {
			return ptMap(childpd, target_va, pteToPa(parentpte), perm);
		}
	}
	return 0;
}

/**
 * 基于传入的线程 fork 出一个子线程并加入调度队列
 */
u64 td_fork(thread_t *td, u64 childsp) {
	// 申请一个新的内核线程
	thread_t *child = td_alloc();

	// 使用写时复制映射父线程的用户线程地址空间
	pdWalk(td->td_pt, duppage, NULL, child->td_pt);
	// 复制父线程的用户现场
	*(child->td_trapframe) = *(td->td_trapframe);
	// 设置子线程的栈顶
	if (childsp != 0) {
		child->td_trapframe->sp = childsp;
	}
	// 设置子线程的返回值
	child->td_trapframe->a0 = 0;

	// 父线程的内核线程信息不用复制，使用新内核线程的入口直接调度
	child->td_status = RUNNABLE;
	safestrcpy(child->td_name, td->td_name, MAX_PROC_NAME_LEN);

	// 复制父线程的文件信息 todo

	// 将子线程加入调度队列
	tdq_critical_enter(&thread_runq);
	TAILQ_INSERT_TAIL(&thread_runq.tq_head, child, td_runq);

	mtx_unlock(&child->td_lock);
	tdq_critical_exit(&thread_runq);

	return child->td_tid; // todo: 之后需要换成 pid
}
