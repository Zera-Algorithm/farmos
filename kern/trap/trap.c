#include <dev/plic.h>
#include <dev/sbi.h>
#include <dev/timer.h>
#include <dev/virtio.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lock/spinlock.h>
#include <mm/memlayout.h>
#include <param.h>
#include <proc/proc.h>
#include <proc/schedule.h>
#include <proc/sleep.h>
#include <riscv.h>
#include <trap/trap.h>
#include <types.h>

extern void kernelvec();
extern void syscallEntry(Trapframe *tf);

#define SCAUSE_EXCEPTION 0
#define SCAUSE_INTERRUPT 1
#define INTERRUPT_TIMER 5
#define INTERRUPT_EXTERNEL 9

extern char trampoline[], userVec[], userRet[];

char *excCause[] = {"Instruction address misaligned",
		    "Instruction access fault",
		    "Illegal instruction",
		    "Breakpoint",
		    "Load address misaligned",
		    "Load access fault",
		    "Store address misaligned",
		    "Store access fault",
		    "Environment call from U-mode",
		    "Environment call from S-mode",
		    "Undefined Exception",
		    "Environment call from M-mode",
		    "Instruction page fault",
		    "Load page fault",
		    "Undefined Exception",
		    "Store page fault"};

/**
 * @brief 内核态异常入口
 * @note 中断：处理中断（但内核一般会屏蔽中断，所以这个大概率不发生）
 * @note 异常：直接panic
 * @note 内核态中断直接返回即可，不需要切换页表，所以也不需要实现在trampoline中
 */
void kerneltrap(RawTrapFrame *tf) {
	// 获取陷入中断的原因
	uint64 scause = r_scause();
	uint64 type = (scause >> 63ul);
	uint64 excCode = (scause & ((1ul << 63) - 1));

	loga("kernel trap: cpu = %d, type = %ld, excCode = %ld\n", cpuid(), type, excCode);
	if (type == SCAUSE_INTERRUPT) {
		if (excCode == INTERRUPT_TIMER) {
			loga("timer interrupt on CPU %d!\n", cpuid());
			timerSetNextTick();
		} else if (excCode == INTERRUPT_EXTERNEL) {
			loga("externel interrupt on CPU %d!\n", cpuid());
			int irq = plicClaim();

			if (irq == VIRTIO0_IRQ) {
				// Note: call virtio intr handler
				loga("[cpu %d] catch virtio intr\n", cpuid());
				virtio_disk_intr();
			} else {
				loga("[cpu %d] unknown externel interrupt irq = %d\n", cpuid(),
				     irq);
			}

			if (irq) {
				plicComplete(irq);
			}
		} else {
			loga("unknown interrupt.\n");
		}
	} else {
		printf("sp = 0x%016lx, ra = 0x%016lx\n", tf->sp, tf->ra);
		panic("uncaught exception.\n"
		      "\tcpu: %d\n"
		      "\tExcCode: %d\n"
		      "\tCause: %s\n"
		      "\tSepc: 0x%016lx (kern/kernel.asm)\n"
		      "\tStval(bad memory address): 0x%016lx\n",
		      cpuid(), excCode, excCause[excCode], r_sepc(), r_stval());
	}
}

/**
 * @brief 当写入一个不可写的页时，会触发Store page fault，excCode=15;
 * @param stval 当发生页错误时，指代发生页错误的地址
 */
void pageFaultHandler(int excCode, u64 epc, u64 stval) {
	log(LEVEL_GLOBAL, "A page fault occurred on: EPC = 0x%08lx, STVAL = 0x%016lx\n", epc,
	    stval);
	u64 badAddr = stval & ~(PAGE_SIZE - 1);
	struct Proc *proc = myProc();

	Pte pte = ptLookup(proc->pageTable, badAddr);
	if (pte == 0) {
		panic("Can\'t find page addr 0x%016lx.\n", badAddr);
	}

	if ((pte & PTE_U) == 0) {
		panic("Write to kernel page! Panic!\n");
	}

	if (pte & PTE_W) {
		panic("It is a PTE_W page! No way to handle!\n");
	}

	log(LEVEL_GLOBAL, "write to a not PTE_W page.\n");

	if (pte & PTE_COW) {
		u64 newPage = vmAlloc();
		u64 oldPage = pteToPa(pte);
		u64 perm = PTE_PERM(pte);
		memcpy((void *)newPage, (void *)oldPage, PAGE_SIZE);
		panic_on(ptMap(proc->pageTable, badAddr, newPage, (perm ^ PTE_COW) | PTE_W));
		log(LEVEL_GLOBAL, "successfully filled COW page!\n");
	} else {
		panic("Not a PTE_COW Page!\n");
	}
}

/**
 * @brief 处理来自用户的中断、异常或系统调用
 * @note 中断：时钟中断（交给调度器）、硬盘中断、键盘中断（Optional）
 * @note 异常：页写入异常，可以实现COW
 * @note 系统调用：属于异常的一种
 */
void userTrap() {
	// 获取陷入中断的原因
	uint64 scause = r_scause();
	uint64 type = (scause >> 63ul);
	uint64 excCode = (scause & ((1ul << 63) - 1));

	// loga("user trap: cpu = %d, type = %ld, excCode = %ld\n", cpuid(), type, excCode);
	if (type == SCAUSE_INTERRUPT) {

		// 时钟中断
		if (excCode == INTERRUPT_TIMER) {
			// loga("timer interrupt on CPU %d!\n", cpuid());
			timerSetNextTick();
			wakeupProc();
			schedule(0); // 请求调度
		} else {
			loga("unknown interrupt.\n");
		}
	} else {
		if (excCode == EXCCODE_SYSCALL) {
			// 处理系统调用
			syscallEntry(myProc()->trapframe);
		} else if (excCode == EXCCODE_PAGE_FAULT) {
			pageFaultHandler(excCode, r_sepc(), r_stval());
		} else {
			printReg(myProc()->trapframe);
			log(LEVEL_GLOBAL, "Curenv: pid = 0x%08lx, name = %s\n", myProc()->pid,
			    myProc()->name);
			panic("uncaught exception.\n"
			      "\tcpu: %d\n"
			      "\tExcCode: %d\n"
			      "\tCause: %s\n"
			      "\tSepc: 0x%016lx (kern/kernel.asm)\n"
			      "\tStval(bad memory address): 0x%016lx\n",
			      cpuid(), excCode, excCause[excCode], r_sepc(), r_stval());
		}
	}

	// 返回用户态
	userTrapReturn();
}

/**
 * @brief 从内核态返回某个用户的用户态
 */
void userTrapReturn() {
	// loga("userTrap return begins:\n");
	/**
	 * @brief 获取当前CPU上应当运行的下一个进程
	 * @note 若发生进程切换，需要更改CPU上的下一个进程，以在这里切换
	 */
	struct Proc *p = myProc();

	// 关中断，避免中断对S态到U态转换的干扰
	intr_off();

	// 加载trapoline地址
	u64 trampolineUserVec = TRAMPOLINE + (userVec - trampoline);
	w_stvec(trampolineUserVec);

	/**
	 * @brief 通过trapframe设置与用户态共享的一些数据，以便在trampoline中使用
	 * @note 包括：页目录satp、trap handler、hartid
	 */
	p->trapframe->kernel_satp = r_satp();
	p->trapframe->trap_handler = (u64)userTrap;
	p->trapframe->hartid = cpuid();

	extern char stack0[];
	p->trapframe->kernel_sp = (u64)stack0 + PAGE_SIZE * (cpuid() + 1);

	// 设置S态Previous Mode and Interrupt Enable，
	// 以在sret时恢复状态
	u64 sstatus = r_sstatus();
	sstatus &= ~SSTATUS_SPP; // SPP = 0: 用户状态
	sstatus |= SSTATUS_SPIE; // SPIE = 1: 开中断
	w_sstatus(sstatus);

	// 恢复epc
	w_sepc(p->trapframe->epc);

	u64 satp = MAKE_SATP(p->pageTable);

	u64 trampolineUserRet = TRAMPOLINE + (userRet - trampoline);
	// loga("goto trampoline, func = 0x%016lx\n", trampolineUserRet);
	((void (*)(u64))trampolineUserRet)(satp);
}

// 设置异常处理跳转入口
void trapInitHart(void) {
	w_stvec((uint64)kernelvec);
}
