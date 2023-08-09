#include <dev/dtb.h>
#include <dev/interface.h>
#include <dev/plic.h>
#include <dev/rtc.h>
#include <dev/sbi.h>
#include <dev/timer.h>
#include <fs/buf.h>
#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/vfs.h>
#include <futex/futex.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <mm/kmalloc.h>
#include <mm/mmu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <param.h>
#include <proc/cpu.h>
#include <proc/proc.h>
#include <proc/sched.h>
#include <proc/thread.h>
#include <proc/tsleep.h>
#include <ipc/shm.h>
#include <riscv.h>
#include <signal/itimer.h>
#include <signal/signal.h>
#include <trap/trap.h>
#include <types.h>

#ifdef SIFIVE
#define IGNORE_HART0 1
#else
#define IGNORE_HART0 0
#endif

// #define SINGLE

// 用于记录当前哪个核已被启动
volatile static int hart_started[NCPU];
// 用于在启动阶段标识一个核是否是第一个被启动的核
volatile static int hart_first = 1;
// 用于阻塞其他核，直到第一个核完成初始化
volatile static int kern_inited = 0;

/**
 * @brief 注意：
 * 对hart_started[]数组和kern_inited的访问（尤其是写入）绝对不能重排序
 * 最好的方式是加锁，其次是在访问的前后都加fence指令，保证该条指令不会提前或者延后
 */

static inline void hart_set_started() {
	__sync_synchronize();
	hart_started[cpu_this_id()] = 1;
	__sync_synchronize();
}

static inline void hart_set_clear() {
	__sync_synchronize();
	for (int i = 0; i < NCPU; i++) {
		hart_started[i] = 0;
	}
	__sync_synchronize();
}

static inline void hart_start_all() {
#ifndef SINGLE
	for (int i = IGNORE_HART0 ? 1 : 0; i < NCPU; i++) {
		if (!hart_started[i]) {
			SBI_HART_START(i, 0x80200000, 0);
			unsigned long mask = (1 << i);
			SBI_SEND_IPI(&mask, 0);
			printf("Hart %d try to start hart %d\n", cpu_this_id(), i);
		}
	}
#endif
}

static inline void hart_wait_all() {
#ifndef SINGLE
	printf("Hart %d is waiting\n", cpu_this_id());
	while (1) {
		__sync_synchronize();
		int all_started = 1;
		for (int i = IGNORE_HART0 ? 1 : 0; i < NCPU; i++) {
			if (!hart_started[i]) {
				all_started = 0;
				break;
			}
		}
		if (all_started) {
			break;
		}
	}
#endif
}

static inline void wait_for_kern_init() {
	while (!kern_inited) {
		__sync_synchronize();
	}
	printf("Hart %d started.\n", cpu_this_id());
}

static inline void kern_init_done() {
	__sync_synchronize();
	kern_inited = 1;
	__sync_synchronize();
}

static inline void kern_load_process() {
	// PROC_CREATE(test_printf, "test1");
	// PROC_CREATE(test_printf, "test2");
	// PROC_CREATE(test_printf, "test3");
	// PROC_CREATE(test_pthread, "test_pthread");
	// PROC_CREATE(test_clone, "test_clone");
	// PROC_CREATE(test_pipe, "test_pipe");
	// PROC_CREATE(test_file, "test_file");
	PROC_CREATE(test_busybox, "test_busybox");
	// PROC_CREATE(test_setitimer, "test_setitimer");
	// PROC_CREATE(test_while, "test_while");
	// PROC_CREATE(test_futex, "test_futex");
}

static inline void hart_init() {
	vmEnable(); // turn on paging
	printf("Hart %d's vm is enabled, booting.\n", cpu_this_id());
	trapInitHart(); // install kernel trap vector
	timerInit();

	plicInitHart(); // 启动中断控制器，开始接收中断
}

static inline void logo() {
	printf("\n");
	printf("8888888888     d8888 8888888b.  888b     d888  .d88888b.   .d8888b.\n");
	printf("888           d88888 888   Y88b 8888b   d8888 d88P\" \"Y88b d88P  Y88b\n");
	printf("888          d88P888 888    888 88888b.d88888 888     888 Y88b.\n");
	printf("8888888     d88P 888 888   d88P 888Y88888P888 888     888  \"Y888b.\n");
	printf("888        d88P  888 8888888P\"  888 Y888P 888 888     888     \"Y88b.\n");
	printf("888       d88P   888 888 T88b   888  Y8P  888 888     888       \"888\n");
	printf("888      d8888888888 888  T88b  888   \"   888 Y88b. .d88P Y88b  d88P\n");
	printf("888     d88P     888 888   T88b 888       888  \"Y88888P\"   \"Y8888P\"\n");
	printf("\n");
}

// start() jumps here in supervisor mode on all CPUs.
void main() {
	if (hart_first == 1) {
		hart_first = 0;
		__sync_synchronize();

		// 初始化串口
		cons_init();
		logo();
		printf("FarmOS kernel is booting (on hart %d) total: %d\n", cpu_this_id(), NCPU);

		// 读取 dtb
		#ifdef QEMU
		parseDtb();
		#else
		extern struct MemInfo memInfo;
		memInfo.size = 16 * 1024ul * 1024ul * 1024ul;
		#endif

		extern struct MemInfo memInfo;
		printf("mem size: %d GB.\n", memInfo.size / 1024ul / 1024ul / 1024ul);

		printf("dtb init success!\n");

		// 内存管理机制初始化
		pmmInit();
		printf("pmm init success!\n");
		vmmInit();
		printf("vmm init success!\n");

		// 初始化核心（开启分页、设置内核异常向量、初始化 timer/plic）
		hart_init();

		// 进程管理机制初始化
		thread_init();
		proc_init();
		sig_init();
		futexevent_init();
		tsleep_init();
		log(LEVEL_GLOBAL, "proc init done\n");

		// 其它初始化
		dev_init();
		plicInit();	// 设置中断控制器
		fd_init();	// include kload lock init
		kmalloc_init();
		shm_init();
		log(LEVEL_GLOBAL, "kmalloc_init done\n");
		socket_init();
		log(LEVEL_GLOBAL, "socket_init done\n");
		itimer_init();
		printf("FarmOS kernel boot end, start all harts.\n");

		// 加载初始化进程
		kern_load_process();

		// 启动其它核心
		hart_set_clear();
		hart_set_started();
		hart_start_all(); // 单核时，这里会直接跳过

		// 避免重排序，hart_start_all一定要在init_done之前
		__sync_synchronize();

		// 解除对其它核心的阻塞，等待其它核心完成初始化
		kern_init_done();
		hart_wait_all(); // 单核时，这里会直接跳过
	} else {
		wait_for_kern_init();
		// 初始化核心（开启分页、设置内核异常向量、初始化 timer/plic）
		hart_init();
		// 等待其它核心完成初始化
		hart_set_started();
		hart_wait_all();
	}

	sched_init();

	while (1) {
		;
	}
}
