#include <dev/dtb.h>
#include <dev/plic.h>
#include <dev/rtc.h>
#include <dev/sbi.h>
#include <dev/virtio.h>
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
// 用于记录当前哪个核已被启动
volatile static int isStarted[NCPU];
// 用于在启动阶段标识一个核是否是第一个被启动的核
volatile static int isFirstHart = 1;

void testProcRun();

/**
 * @brief 启动剩余的 hart
 */
void hartInit() {
	// 启动那些未被启动的核
	for (int i = 0; i < NCPU; i++) {
		if (!isStarted[i]) {
			SBI_HART_START(i, KERNBASE, 0);
		}
	}
}

extern void trapInitHart();

// start() jumps here in supervisor mode on all CPUs.
void main() {
	if (isFirstHart == 1) {
		isFirstHart = 0;

		// 初始化 isStarted（原因是初始时.BSS段可能不会被赋值为0）
		for (int i = 0; i < NCPU; i++) {
			isStarted[i] = 0;
		}
		isStarted[cpuid()] = 1; // 标记自己已启动

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
		hartInit(); // 启动其他Hart（成功分页后再启动其他核）

		__sync_synchronize();
		started = 1;

		// 等待其它核全部启动完毕再开始virtio测试
		while (1) {
			__sync_synchronize();
			int tot = 0;
			for (int i = 0; i < NCPU; i++) {
				tot += isStarted[i];
			}
			if (tot == NCPU) {
				break;
			}
		}

		// virtio驱动读写测试
		virtioTest();

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

		plicInitHart();   // 启动中断控制器，开始接收中断

		isStarted[cpuid()] = 1;
		__sync_synchronize(); // TODO: 封装一层
	}

	while (1) {
		;
	}
}
