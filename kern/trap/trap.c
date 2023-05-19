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

#define SCAUSE_EXCEPTION 0
#define SCAUSE_INTERRUPT 1

#define INTERRUPT_TIMER 5

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

// 内核态中断入口
void kerneltrap() {
	// 获取陷入中断的原因
	uint64 scause = r_scause();
	uint64 type = (scause >> 63ul);
	uint64 excCode = (scause & ((1ul << 63) - 1));

	log("trap: cpu = %d, type = %ld, excCode = %ld\n", cpuid(), type, excCode);
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

// 设置异常处理跳转入口
void trapinithart(void) {
	w_stvec((uint64)kernelvec);
}
