#include <defs.h>
#include <dev/dtb.h>
#include <dev/sbi.h>
#include <mm/kalloc.h>
#include <mm/memlayout.h>
#include <mm/memory.h>
#include <param.h>
#include <riscv.h>
#include <types.h>

volatile static int started = 0;

void hartInit() {
	SBI_HART_START(1, 0x80200000, 0);
	SBI_HART_START(2, 0x80200000, 0);
}

extern void trapinithart();

// start() jumps here in supervisor mode on all CPUs.
void main() {
	if (cpuid() == 0) {
		// consoleinit();
		printfinit();
		parseDtb();
		printf("\n");
		printf("FarmOS kernel is booting\n");
		printf("\n");
		// kinit();         // physical page allocator
		initKernelMemory(); // 初始化内核页表
		enablePagingHart(); // 开启分页
		log("Finish Paging!\n");
		// procinit();      // process table
		// trapinit();      // trap vectors
		kinit();
		trapinithart(); // install kernel trap vector
		timerInit();	// 初始化核内时钟
		// plicinit();      // set up interrupt controller
		// plicinithart();  // ask PLIC for device interrupts
		// binit();         // buffer cache
		// iinit();         // inode table
		// fileinit();      // file table
		// virtio_disk_init(); // emulated hard disk
		// userinit();      // first user process
		// *(char *)0 = 0;  // 尝试触发异常
		hartInit(); // 启动其他Hart（成功分页后再启动其他核）
		__sync_synchronize();
		started = 1;
	} else {
		while (started == 0) {
			;
		}
		__sync_synchronize();
		enablePagingHart(); // turn on paging
		printf("hart %d is starting\n", cpuid());
		trapinithart(); // install kernel trap vector
		timerInit();

		// plicinithart();   // ask PLIC for device interrupts
	}

	while (1) {
		;
	}
	// scheduler();
}
