#include <lib/elf.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <lib/transfer.h>
#include <mm/kmalloc.h>
#include <mm/vmm.h>
#include <mm/vmtools.h>
#include <param.h>
#include <proc/proc.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <proc/procarg.h>

threadq_t thread_runq;
threadq_t thread_freeq;
threadq_t thread_sleepq;
thread_t threads[NPROC];

proclist_t proc_freelist;
proc_t procs[NPROC];

void *kstacks;

void thread_init() {
	extern mutex_t td_tid_lock;
	mtx_init(&td_tid_lock, "td_tid_lock", false, MTX_SPIN);
	mtx_init(&wait_lock, "wait_lock", false, MTX_SPIN);
	mtx_init(&thread_runq.tq_lock, "thread_runq", false, MTX_SPIN);
	mtx_init(&thread_freeq.tq_lock, "thread_freeq", false, MTX_SPIN);
	mtx_init(&thread_sleepq.tq_lock, "thread_sleepq", false, MTX_SPIN);
	TAILQ_INIT(&thread_runq.tq_head);
	TAILQ_INIT(&thread_freeq.tq_head);
	TAILQ_INIT(&thread_sleepq.tq_head);
	for (int i = NPROC - 1; i >= 0; i--) {
		thread_t *td = &threads[i];
		// 插入空闲线程队列
		TAILQ_INSERT_HEAD(&thread_freeq.tq_head, td, td_freeq);
		// 初始化线程锁
		mtx_init(&td->td_lock, "thread", false, MTX_SPIN | MTX_RECURSE);
		// 初始化线程内核栈
		td->td_kstack = (u64)kstacks + TD_KSTACK_SIZE * i;
		// 将内核线程栈映射到内核页表
		extern pte_t *kernPd;
		for (int j = 0; j < TD_KSTACK_PAGE_NUM; j++) {
			u64 pa = td->td_kstack + j * PAGE_SIZE;
			u64 va = TD_KSTACK(i) + j * PAGE_SIZE;
			panic_on(ptMap(kernPd, va, pa, PTE_R | PTE_W));
		}
	}
}

void proc_init() {
	extern mutex_t pid_lock;
	mtx_init(&pid_lock, "pid_lock", false, MTX_SPIN);
	mtx_init(&proc_freelist.pl_lock, "proc_freelist", false, MTX_SPIN);
	LIST_INIT(&proc_freelist.pl_list);
	for (int i = NPROC - 1; i >= 0; i--) {
		proc_t *p = &procs[i];
		// 插入空闲进程队列
		LIST_INSERT_HEAD(&proc_freelist.pl_list, p, p_list);
		// 初始化进程锁
		mtx_init(&p->p_lock, "proc", false, MTX_SPIN | MTX_RECURSE);
		// 初始化进程的线程队列
		TAILQ_INIT(&p->p_threads);
		// 初始化进程的子进程队列
		LIST_INIT(&p->p_children);
		// 初始化进程的父进程
		p->p_parent = NULL;
		// 初始化进程的页表
		p->p_pt = NULL;
		// 初始化进程的trapframe
		p->p_trapframe = NULL;
		// 初始化进程的用户栈
		p->p_brk = 0;
	}
}

/**
 * @brief 分配并初始化一个新的用户空间页表
 * @note 申请了用户页表、用户 Trapframe
 */
void proc_initupt(proc_t *p) {
	// 分配页表
	pte_t upt = kvmAlloc();
	p->p_pt = (pte_t *)upt;

	// TRAMPOLINE
	extern char trampoline[];
	// 由于TRAMPOLINE是用户与内核共享的空间，因此需要赋以 PTE_G 全局位
	panic_on(ptMap(p->p_pt, TRAMPOLINE, (u64)trampoline, PTE_R | PTE_X | PTE_G));

	// signal TRAMPOLINE
	extern char user_sig_return[];
	panic_on(ptMap(p->p_pt, SIGNAL_TRAMPOLINE, (u64)user_sig_return, PTE_R | PTE_X | PTE_U));

	// 该进程的trapframe
	p->p_trapframe = (trapframe_t *)vmAlloc();
	panic_on(ptMap(p->p_pt, TRAPFRAME, (u64)p->p_trapframe, PTE_R | PTE_W));
}

/**
 * @brief 分配并初始化一个新的用户空间栈
 * @note 申请了新的用户栈，并将其映射到用户页表，同时初始化用户栈指针
 */
void proc_initustack(proc_t *p, thread_t *inittd) {
	// 分配用户栈空间
	for (int i = 0; i < TD_USTACK_INIT_PAGE_NUM; i++) {
		u64 pa = vmAlloc();
		u64 va = TD_USTACK_INIT_BOTTOM + i * PAGE_SIZE;
		panic_on(ptMap(p->p_pt, va, pa, PTE_R | PTE_W | PTE_U));
	}
	// 分配可拓展的用户栈空间
	for (int i = 0; i < TD_USTACK_EXTEND_PAGE_NUM; i++) {
		u64 va = TD_USTACK_BOTTOM + i * PAGE_SIZE;
		panic_on(ptMap(p->p_pt, va, 0, PTE_R | PTE_W | PTE_U | PTE_PASSIVE));
	}
	// 初始化用户栈空间指针
	inittd->td_trapframe.sp = USTACKTOP;
	p->p_brk = 0;
}

void proc_recycleupt(proc_t *p) {
	// 解引用全部用户页表
	pdWalk(p->p_pt, vmUnmapper, kvmUnmapper, NULL);
	p->p_pt = 0;
	p->p_brk = 0;
}


/**
 * @brief 将给定的进程运行参数压入用户栈
 * @param td 线程指针
 * @param argpt 存储exec的新进程栈上内容的临时页表指针
 * @param argc 参数数量
 * @param argv 用户空间内的参数指针
 * @param envp 用户空间内的环境变量指针。为0表示没有环境变量
 */
stack_arg_t proc_setustack(thread_t *td, pte_t *argpt, u64 argc, char **argv, u64 envp,
		    argv_callback_t callback) {
	pte_t *src_pt = td->td_proc->p_pt;
	// argv和envp需要并列在一个数组里面，以实现头部的对齐
	char **argvbuf = kmalloc(MAXARG * sizeof(char *));

	// 1. 拷入argv数组
	u64 len_argv =
	    push_uarg_array(src_pt, argpt, argv, &td->td_trapframe.sp, argvbuf, callback);

	// 2. 拷入envp数组
	u64 len_envp = push_uarg_array(src_pt, argpt, (char **)envp, &td->td_trapframe.sp, argvbuf + len_argv,
			NULL);

	// 加环境变量不能在内核exec时候加，而是需要在用户态exec（test——busybox）时候加
	// 否则容易重复加环境变量
	// u64 len_envp = push_karg_array(argpt, (char *[]){"LD_LIBRARY_PATH=/", NULL},
	// 			       &td->td_trapframe.sp, argvbuf + len_argv);

	u64 total_len = len_argv + len_envp;
	argc = len_argv - 1;

	return (stack_arg_t){argvbuf, total_len, argc};

	// TODO：挪到最后loadCode结束后处理
	// argvbuf(need kfree), total_len, argc
	// // 3. 拷入辅助数组
	// /**
	//  * 辅助数组的字段由type和value两个组成，都是u64类型
	//  * 辅助数组以NULL, NULL表示结束
	//  */
	// // TODO：最后再压入辅助数组
	// append_auxiliary_vector(argvbuf, (u64[]){AT_HWCAP, 0}, &total_len);
	// append_auxiliary_vector(argvbuf, (u64[]){AT_PAGESZ, PAGE_SIZE}, &total_len);
	// append_auxiliary_vector(argvbuf, (u64[]){AT_NULL, AT_NULL}, &total_len);
	// // reference: glibc

	// // 4. 将argvbuf压入用户栈
	// push_data(argpt, &td->td_trapframe.sp, argvbuf, total_len * sizeof(char *), true);

	// // 5. 将参数数量压入用户栈
	// push_data(argpt, &td->td_trapframe.sp, &argc, sizeof(u64), false);
}
