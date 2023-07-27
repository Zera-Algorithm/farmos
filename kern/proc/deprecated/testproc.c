#include <lib/log.h>
#include <lib/string.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <trap/trap.h>

/**
 * @brief 测试版的procRun，运行一个进程
 * @note 此处与MOS不同。MOS是通过位于内核态的时钟中断来激活第一个进程的。
 * 		而我们的内核用户态时钟中断是与内核态时钟中断分开的，
 * 		调度只能在用户态的时钟中断中实现。所以需要手动运行第一个进程
 */

// 下面的测试程序仅包含两条指令：j spin; nop
u8 initcode[] = {0x01, 0xa0, 0x01, 0x00};

const char *testProcName[] = {
    "init", "testProc1", "testProc2", "testProc3", "testProc4", "testProc5", "testProc6", "testProc7",
};

// deprecated
void testProcRun(int index) {
	log(DEFAULT, "start init...\n");

	// 1. 设置proc
	thread_t *proc = td_alloc();
	mtx_set(&proc->td_lock, testProcName[index], false);
	strncpy(proc->td_name, testProcName[index], sizeof(proc->td_name));

	// 4. 映射代码段
	void *code = (void *)vmAlloc();
	memcpy(code, initcode, sizeof(initcode));
	panic_on(ptMap(proc->td_pt, 0, (u64)code, PTE_R | PTE_X | PTE_U)); // 需要设置PTE_U允许用户访问
	assert(pteToPa(ptLookup(proc->td_pt, 0)) == (u64)code);

	// 5. 设置Trapframe
	proc->td_trapframe->epc = 0;
	proc->td_trapframe->sp = PAGE_SIZE;

	extern threadq_t thread_runq;
	tdq_critical_enter(&thread_runq);
	TAILQ_INSERT_HEAD(&thread_runq.tq_head, proc, td_runq);
	tdq_critical_exit(&thread_runq);

	// 6. 设置进程状态
	proc->td_status = RUNNABLE;
	mtx_unlock(&proc->td_lock);
}
