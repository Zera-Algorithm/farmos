#include <lib/log.h>
#include <lib/string.h>
#include <mm/vmm.h>
#include <param.h>
#include <proc/thread.h>

threadq_t thread_runq;
threadq_t thread_freeq;
threadq_t thread_sleepq;
thread_t threads[NPROC];
void *kstacks;

void thread_init() {
	extern mutex_t td_tid_lock;
	mtx_init(&td_tid_lock, "td_tid_lock", false);
	mtx_init(&thread_runq.tq_lock, "thread_runq", false);
	mtx_init(&thread_freeq.tq_lock, "thread_freeq", false);
	mtx_init(&thread_sleepq.tq_lock, "thread_sleepq", false);
	TAILQ_INIT(&thread_runq.tq_head);
	TAILQ_INIT(&thread_freeq.tq_head);
	TAILQ_INIT(&thread_sleepq.tq_head);
	for (int i = NPROC - 1; i >= 0; i--) {
		thread_t *td = &threads[i];
		// 插入空闲线程队列
		TAILQ_INSERT_HEAD(&thread_freeq.tq_head, td, td_freeq);
		// 初始化线程锁
		mtx_init(&td->td_lock, "thread", false);
		// 初始化线程内核栈
		td->td_kstack = (u64)kstacks + KTHREAD_STACK_SIZE * i;
		// 将内核线程栈映射到内核页表
		extern pte_t *kernPd;
		for (int j = 0; j < KTHREAD_STACK_PAGE; j++) {
			u64 pa = td->td_kstack + j * PAGE_SIZE;
			u64 va = KTHREAD_STACK(i) + j * PAGE_SIZE;
			panic_on(ptMap(kernPd, va, pa, PTE_R | PTE_W));
		}
	}
}

/**
 * @brief 分配并初始化一个新的用户空间页表
 * @note 申请了用户页表、用户栈、用户 Trapframe
 */
void td_initupt(thread_t *proc) {
	// 分配页表
	pte_t upt = kvmAlloc();
	proc->td_pt = (pte_t *)upt;

	// TRAMPOLINE
	extern char trampoline[];
	// 由于TRAMPOLINE是用户与内核共享的空间，因此需要赋以 PTE_G 全局位
	panic_on(ptMap(proc->td_pt, TRAMPOLINE, (u64)trampoline, PTE_R | PTE_X | PTE_G));

	// 该进程的trapframe
	proc->td_trapframe = (struct trapframe *)vmAlloc();
	panic_on(ptMap(proc->td_pt, TRAPFRAME, (u64)proc->td_trapframe, PTE_R | PTE_W));
}

/**
 * @author zrp
 * @brief 初始化proc的页表
 * @note 需要配置Trampoline、Trapframe、栈、页表（自映射）的映射
 * @param proc 进程指针
 * @return int <0表示出错
 */
int __td_initupt(thread_t *proc, u64 stackTop) {
	// 分配页表
	u64 pa = kvmAlloc();
	proc->td_pt = (Pte *)pa;

	// TRAMPOLINE
	extern char trampoline[];
	// 由于TRAMPOLINE是用户与内核共享的空间，因此需要赋以 PTE_G 全局位
	unwrap(ptMap(proc->td_pt, TRAMPOLINE, (u64)trampoline, PTE_R | PTE_X | PTE_G));

	// 该进程的trapframe
	proc->td_trapframe = (struct trapframe *)vmAlloc();
	unwrap(ptMap(proc->td_pt, TRAPFRAME, (u64)proc->td_trapframe, PTE_R | PTE_W));

	// 该进程的栈
	u64 stack = vmAlloc();

	// 如果指定了stackTop，反而不需要分配
	// 因为只有fork时才会提供stackTop，这时用户态肯定已经准备好了
	if (stackTop == 0) {
		stackTop = USTACKTOP;
		log(PROC_MODULE, "alloc stack address = 0x%08lx\n", stackTop - PAGE_SIZE);
		unwrap(ptMap(proc->td_pt, stackTop - PAGE_SIZE, stack, PTE_R | PTE_W | PTE_U));
	}

	u64 *top = (void *)stack;
	*(top - 1) = 0; // argv
	*(top - 2) = 0; // argc
	// TODO: 为什么这里写错了还能跑通？

	// 为argc和argv留出位置
	proc->td_trapframe->sp = stackTop - sizeof(long) - sizeof(long);

	return 0;
}