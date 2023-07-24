#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
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
		} else if (perm & PTE_U) {
			return ptMap(childpd, target_va, pteToPa(parentpte), perm);
		} else {
			error("duppage: invalid perm %x\n", perm);
		}
	}
	return 0;
}

static void proc_fork_name_debug(thread_t *childtd) {
	// FOR DEBUG
	int len = strlen(childtd->td_name);
	if (childtd->td_name[len - 1] == ']') {
		const char *pleft = strchr(childtd->td_name, '[');
		char *p = (char *)pleft;
		int sum = 0;
		while (*++p != ']') {
			sum = sum * 10 + (*p - '0');
		}
		sum++;
		p = (char *)pleft;
		*p = '\0';
		strcat(childtd->td_name, "[");
		if (sum >= 100) {
			int temp = (sum / 100) % 10;
			strcat(childtd->td_name, (char[]){'0' + temp, '\0'});
		}
		if (sum >= 10) {
			int temp = (sum / 10) % 10;
			strcat(childtd->td_name, (char[]){'0' + temp, '\0'});
		}
		int temp = sum % 10;
		strcat(childtd->td_name, (char[]){'0' + temp, '\0'});
		strcat(childtd->td_name, "]");
	} else {
		strcat(childtd->td_name, "[1]");
	}
}

u64 td_fork(thread_t *td, u64 childsp, u64 ptid, u64 tls, u64 ctid) {
	proc_t *p = td->td_proc;
	thread_t *childtd = td_alloc();

	// 将线程与进程控制块绑定
	proc_lock(p);
	proc_addtd(p, childtd);
	proc_unlock(p);

	// 设置新线程的参数
	safestrcpy(childtd->td_name, td->td_name, MAX_PROC_NAME_LEN);
	proc_fork_name_debug(childtd);
	childtd->td_status = RUNNABLE;

	// 复制父线程的用户现场
	childtd->td_trapframe = td->td_trapframe;
	// 设置子线程的栈顶
	if (childsp != 0) {
		childtd->td_trapframe.sp = childsp;
	}
	// 设置子线程的返回值
	childtd->td_trapframe.a0 = 0;
	childtd->td_trapframe.tp = tls;
	if (ptid != 0) {
		copyOut(ptid, (void *)&childtd->td_tid, sizeof(u32));
	}
	childtd->td_ctid = ctid;

	// 将子进程的初始线程加入调度队列
	tdq_critical_enter(&thread_runq);
	TAILQ_INSERT_TAIL(&thread_runq.tq_head, childtd, td_runq);
	mtx_unlock(&childtd->td_lock);
	tdq_critical_exit(&thread_runq);
	return childtd->td_tid;
}

/**
 * 基于传入的线程 fork 出一个新的进程（含一个初始化线程）并加入调度队列
 */
u64 proc_fork(thread_t *td, u64 childsp) {
	proc_t *p = td->td_proc;

	// 获取一个进程和一个线程
	proc_t *childp = proc_alloc();
	thread_t *childtd = td_alloc();

	// 将线程与进程控制块绑定
	proc_lock(p);
	proc_addtd(childp, childtd);

	// 父进程操作
	// 使用写时复制映射父进程的用户线程地址空间
	pdWalk(p->p_pt, duppage, NULL, childp->p_pt);
	childp->p_brk = p->p_brk;
	safestrcpy(childtd->td_name, td->td_name, MAX_PROC_NAME_LEN);
	proc_fork_name_debug(childtd);
	// 复制父线程的文件信息
	fork_thread_fs(&p->p_fs_struct, &childp->p_fs_struct);

	// 父线程的内核线程信息不用复制，使用新内核线程的入口直接调度
	childtd->td_status = RUNNABLE;

	// 创建进程的父子关系
	childp->p_parent = p;
	LIST_INSERT_HEAD(&p->p_children, childp, p_sibling);
	proc_unlock(p);

	// 复制父线程的用户现场
	childtd->td_trapframe = td->td_trapframe;
	// 设置子线程的栈顶
	if (childsp != 0) {
		childtd->td_trapframe.sp = childsp;
	}
	// 设置子线程的返回值
	childtd->td_trapframe.a0 = 0;

	// 将子进程的初始线程加入调度队列
	tdq_critical_enter(&thread_runq);
	TAILQ_INSERT_TAIL(&thread_runq.tq_head, childtd, td_runq);
	mtx_unlock(&childtd->td_lock);
	proc_unlock(childp);
	tdq_critical_exit(&thread_runq);

	return childp->p_pid;
}
