#include <dev/dtb.h>
#include <dev/plic.h>
#include <dev/rtc.h>
#include <dev/sbi.h>
#include <dev/timer.h>
#include <dev/virtio.h>
#include <fs/buf.h>
#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <mm/mmu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <param.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <riscv.h>
#include <types.h>

volatile static int started = 0;
// 用于记录当前哪个核已被启动
volatile static int isStarted[NCPU];
// 用于在启动阶段标识一个核是否是第一个被启动的核
volatile static int isFirstHart = 1;
volatile static int is_fully_started = 0;

void testProcRun(int);

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
extern void sched_init();

// #define SINGLE

// start() jumps here in supervisor mode on all CPUs.
void main() {
	if (isFirstHart == 1) {
		isFirstHart = 0;

		// 初始化 isStarted（原因是初始时.BSS段可能不会被赋值为0）
		printInit();
		log(LEVEL_GLOBAL, "NCPU = %d\n", NCPU);
		for (int i = 0; i < NCPU; i++) {
			isStarted[i] = 0;
		}
		isStarted[cpu_this_id()] = 1; // 标记自己已启动

		// consoleinit();
		log(LEVEL_GLOBAL, "FarmOS kernel is booting (on hart %d)\n", cpu_this_id());
		parseDtb();

		// 内存管理机制初始化
		pmmInit();
		vmmInit();
		// bufInit();

		// initKernelMemory(); // 初始化内核页表
		vmEnable(); // 开启分页
		thread_init();
		trapInitHart(); // install kernel trap vector
		timerInit();	// 初始化核内时钟
		plicInit();	// 设置中断控制器
		plicInitHart(); // 设置本hart的中断控制器
		fd_init();
		// virtio_disk_init(); // emulated hard disk
		// userinit();      // first user process
		// *(char *)0 = 0;  // 尝试触发异常

#ifndef SINGLE
		hartInit(); // 启动其他Hart（成功分页后再启动其他核）
#endif

		__sync_synchronize();

		extern mutex_t mtx_file_load;
		mtx_init(&mtx_file_load, "kload", 0, MTX_SLEEP);

		assert(intr_get() == 0);
		// testProcRun(0);
		// testProcRun(1);
		// testProcRun(2);
		// testProcRun(3);
		// testProcRun(4);
		// testProcRun(5);
		// testProcRun(6);
		// testProcRun(7);
		// TD_CREATE(test_printf, "test1");
		// TD_CREATE(test_printf, "test2");
		// TD_CREATE(test_printf, "test3");
		TD_CREATE(test_clone, "test_clone");
		// TD_CREATE(test_while, "test_while");

		printf("Waiting from Hart %d\n", cpu_this_id());
		started = 1;

#ifndef SINGLE
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
#endif

		printf("hart %d ~~~~~~~~~~~~~~~~~~~\n", cpu_this_id());
	} else {
		while (started == 0) {
			;
		}
		__sync_synchronize();
		printf("hart %d is starting\n", cpu_this_id());
		vmEnable(); // turn on paging
		printf("hart %d is starting\n", cpu_this_id());
		trapInitHart(); // install kernel trap vector
		timerInit();

		plicInitHart(); // 启动中断控制器，开始接收中断

		isStarted[cpu_this_id()] = 1;
		__sync_synchronize(); // TODO: 封装一层

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
		printf("hart %d ~~~~~~~~~~~~~~~~~~~\n", cpu_this_id());
	}

	assert(intr_get() == 0);
	sched_init();

	while (1) {
		;
	}
}
