#include <dev/dtb.h>
#include <dev/plic.h>
#include <dev/rtc.h>
#include <dev/sbi.h>
#include <dev/timer.h>
#include <lib/printf.h>
#include <mm/memlayout.h>
#include <mm/memory.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <param.h>
#include <proc/proc.h>
#include <riscv.h>
#include <types.h>

volatile static int started = 0;

// 用于判断当前核是否是第一个启动的核
volatile static int numStart = 0;

void testProcRun();

void hartInit() {
	SBI_HART_START(1, 0x80200000, 0);
	SBI_HART_START(2, 0x80200000, 0);
}

extern void trapInitHart();

// start() jumps here in supervisor mode on all CPUs.
void main() {
	if (numStart == 0) {
		numStart += 1;
		// consoleinit();
		printInit();
		printf("FarmOS kernel is booting (on hart %d)\n", cpuid());
		parseDtb();
		printf("\n");
		// 内存管理机制初始化
		pmmInit();
		vmmInit();
		// initKernelMemory(); // 初始化内核页表
		enablePagingHart(); // 开启分页
		loga("Finish Paging!\n");
		// procinit();      // process table
		// trapinit();      // trap vectors
		trapInitHart(); // install kernel trap vector
		timerInit();	// 初始化核内时钟
		plicInit();	// 设置中断控制器
		plicInitHart(); // 设置本hart的中断控制器
		// binit();         // buffer cache
		// iinit();         // inode table
		// fileinit();      // file table
		// virtio_disk_init(); // emulated hard disk
		// userinit();      // first user process
		// *(char *)0 = 0;  // 尝试触发异常
		// hartInit(); // 启动其他Hart（成功分页后再启动其他核）
		__sync_synchronize();
		started = 1;

		// testProcRun();
		procInit();
		PROC_CREATE(test_while, 2);

		struct Proc *proc = PROC_CREATE(test_sleep, 1);
		procRun(NULL, proc);
	} else {
		while (started == 0) {
			;
		}
		__sync_synchronize();
		enablePagingHart(); // turn on paging
		printf("hart %d is starting\n", cpuid());
		trapInitHart(); // install kernel trap vector
		timerInit();

		// plicinithart();   // ask PLIC for device interrupts
	}

	while (1) {
		;
	}
}
