#include "trap/trap.h"
#include "defs.h"
#include "dev/sbi.h"
#include "lib/printf.h"
#include "lock/spinlock.h"
#include "mm/memlayout.h"
#include "param.h"
#include "proc/proc.h"
#include "riscv.h"
#include "types.h"

extern void kernelvec();
extern void syscallEntry(Trapframe *tf);

#define SCAUSE_EXCEPTION 0
#define SCAUSE_INTERRUPT 1
#define INTERRUPT_TIMER 5

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
void kerneltrap() {
	// 获取陷入中断的原因
	uint64 scause = r_scause();
	uint64 type = (scause >> 63ul);
	uint64 excCode = (scause & ((1ul << 63) - 1));

	log("kernel trap: cpu = %d, type = %ld, excCode = %ld\n", cpuid(), type, excCode);
	if (type == SCAUSE_INTERRUPT) {
		if (excCode == INTERRUPT_TIMER) {
			log("timer interrupt on CPU %d!\n", cpuid());
			// TODO: 实现更丰富的中断处理
			timerSetNextTick();
		} else {
			log("unknown interrupt.\n");
		}
	} else {
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

	log("user trap: cpu = %d, type = %ld, excCode = %ld\n", cpuid(), type, excCode);
	if (type == SCAUSE_INTERRUPT) {

		// 时钟中断
		if (excCode == INTERRUPT_TIMER) {
			log("timer interrupt on CPU %d!\n", cpuid());
			timerSetNextTick();

			// TODO: call schedule
		} else {
			log("unknown interrupt.\n");
		}
	} else {
		if (excCode == 8) {			   // TODO: literal
			syscallEntry(myProc()->trapframe); // TODO: call do_syscall
		} else {
			if (excCode == 2) {
				u32 *code = (u32 *)pteToPa(ptLookup(myProc()->pageTable, r_sepc()));
				log("code = 0x%08x\n", *code);
			}

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
	// log("userTrap return begins:\n");
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
	// log("goto trampoline, func = 0x%016lx\n", trampolineUserRet);
	((void (*)(u64))trampolineUserRet)(satp);
}

// 设置异常处理跳转入口
void trapInitHart(void) {
	w_stvec((uint64)kernelvec);
}
