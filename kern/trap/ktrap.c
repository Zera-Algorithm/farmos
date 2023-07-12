#include <dev/plic.h>
#include <dev/sbi.h>
#include <dev/timer.h>
#include <dev/virtio.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lock/spinlock.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <param.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <riscv.h>
#include <trap/trap.h>
#include <types.h>

#include <proc/cpu.h>
#define myProc() (cpu_this()->cpu_running)
#define cpuid() (cpu_this_id())

extern void kernelvec();

#define SCAUSE_EXCEPTION 0
#define SCAUSE_INTERRUPT 1
#define INTERRUPT_TIMER 5
#define INTERRUPT_EXTERNEL 9

extern char trampoline[], userVec[], userRet[];

static char *excCause[] = {"Instruction address misaligned",
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

	log(LEVEL_GLOBAL, "kernel trap: cpu = %d, type = %ld, excCode = %ld\n", cpuid(), type,
	    excCode);
	if (type == SCAUSE_INTERRUPT) {
		if (excCode == INTERRUPT_TIMER) {
			log(DEFAULT, "timer interrupt on CPU %d!\n", cpuid());
			timerSetNextTick();
		} else if (excCode == INTERRUPT_EXTERNEL) {
			log(DEFAULT, "externel interrupt on CPU %d!\n", cpuid());
			int irq = plicClaim();

			if (irq == VIRTIO0_IRQ) {
				// Note: call virtio intr handler
				log(DEFAULT, "[cpu %d] catch virtio intr\n", cpuid());
				virtio_disk_intr();
			} else {
				log(DEFAULT, "[cpu %d] unknown externel interrupt irq = %d\n",
				    cpuid(), irq);
			}

			if (irq) {
				plicComplete(irq);
			}
		} else {
			log(DEFAULT, "unknown interrupt.\n");
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

// 设置异常处理跳转入口
void trapInitHart(void) {
	w_stvec((uint64)kernelvec);
}
