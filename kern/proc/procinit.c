#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <mm/vmtools.h>
#include <param.h>
#include <proc/sleep.h>
#include <proc/thread.h>

threadq_t thread_runq;
threadq_t thread_freeq;
threadq_t thread_sleepq;
thread_t threads[NPROC];
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
		mtx_init(&td->td_lock, "thread", false, MTX_SPIN);
		// 初始化子线程队列
		LIST_INIT(&td->td_childlist);
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

/**
 * @brief 分配并初始化一个新的用户空间页表
 * @note 申请了用户页表、用户栈、用户 Trapframe
 */
void td_initupt(thread_t *td) {
	// 分配页表
	pte_t upt = kvmAlloc();
	td->td_pt = (pte_t *)upt;

	// TRAMPOLINE
	extern char trampoline[];
	// 由于TRAMPOLINE是用户与内核共享的空间，因此需要赋以 PTE_G 全局位
	panic_on(ptMap(td->td_pt, TRAMPOLINE, (u64)trampoline, PTE_R | PTE_X | PTE_G));

	// 该进程的trapframe
	td->td_trapframe = (struct trapframe *)vmAlloc();
	panic_on(ptMap(td->td_pt, TRAPFRAME, (u64)td->td_trapframe, PTE_R | PTE_W));
}

/**
 * @brief 分配并初始化一个新的用户空间栈
 * @note 申请了新的用户栈，并将其映射到用户页表，同时初始化用户栈指针
 */
void td_initustack(thread_t *td, u64 ustack) {
	// 分配用户栈空间
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 pa = vmAlloc();
		u64 va = ustack + i * PAGE_SIZE;
		panic_on(ptMap(td->td_pt, va, pa, PTE_R | PTE_W | PTE_U));
	}
	// 初始化用户栈空间指针
	td->td_trapframe->sp = ustack + TD_USTACK_SIZE;
	td->td_brk = 0;
}

void td_recycleupt(thread_t *td) {
	// 解引用全部用户页表
	pdWalk(td->td_pt, vmUnmapper, kvmUnmapper, NULL);
	td->td_pt = 0;
}

/**
 * @brief 将给定的进程运行参数压入用户栈
 * @param argc 参数数量
 * @param argv 用户空间内的参数指针
 */
void td_setustack(thread_t *td, u64 argc, char **argv) {
	// 将参数压入用户栈，无参数时跳过
	char buf[MAXARGLEN + 1];
	char *argvbuf[MAXARG];

	for (int i = argc - 1; i >= 0; i--) {
		// 指向参数字符串的用户地址空间指针
		char *arg;
		copy_in(td->td_pt, (u64)(&argv[i]), &arg, sizeof(char *));
		// 从用户地址空间拷贝参数字符串
		copy_in_str(td->td_pt, (u64)arg, buf, MAXARGLEN);
		buf[MAXARGLEN] = '\0';
		// 计算参数字符串长度
		size_t len = strlen(buf) + 1;
		buf[len - 1] = '\0';
		// 将参数字符串压入用户栈
		td->td_trapframe->sp -= len;
		// 将字符串首地址对齐到 16 字节
		td->td_trapframe->sp -= td->td_trapframe->sp % 16;
		copy_out(td->td_pt, (u64)td->td_trapframe->sp, buf, len);
		// 记录参数字符串的用户地址空间指针
		argvbuf[i] = (char *)td->td_trapframe->sp;
	}
	argvbuf[argc] = NULL;

	// 将参数指针压入用户栈
	td->td_trapframe->sp -= (argc + 1) * sizeof(char *);
	// 将指针数组首地址对齐到 16 字节
	td->td_trapframe->sp -= td->td_trapframe->sp % 16;
	copy_out(td->td_pt, (u64)td->td_trapframe->sp, argvbuf, (argc + 1) * sizeof(char *));

	// 将参数数量压入用户栈
	td->td_trapframe->sp -= sizeof(u64);
	copy_out(td->td_pt, (u64)td->td_trapframe->sp, &argc, sizeof(u64));

	// 将参数放入寄存器
	// 通过 syscall 返回值实现 a0 = argc;
	td->td_trapframe->a1 = td->td_trapframe->sp;
}
