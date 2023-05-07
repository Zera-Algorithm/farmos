#include "SBI.h"
#include "defs.h"
#include "dtb.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

volatile static int started = 0;

void hart_init() {
	SBI_HART_START(1, 0x80200000, 0);
	SBI_HART_START(2, 0x80200000, 0);
}

extern void trapinithart();

// start() jumps here in supervisor mode on all CPUs.
void main() {
	if (cpuid() == 0) {
		// consoleinit();
		printfinit();
		dtb_parser();
		hart_init(); // 启动其他Hart
		printf("\n");
		printf("xv6 kernel is booting\n");
		printf("\n");
		// kinit();         // physical page allocator
		// kvminit();       // create kernel page table
		// kvminithart();   // turn on paging
		// procinit();      // process table
		// trapinit();      // trap vectors
		trapinithart(); // install kernel trap vector
		timerInit();	// 初始化核内时钟
		// plicinit();      // set up interrupt controller
		// plicinithart();  // ask PLIC for device interrupts
		// binit();         // buffer cache
		// iinit();         // inode table
		// fileinit();      // file table
		// virtio_disk_init(); // emulated hard disk
		// userinit();      // first user process
		__sync_synchronize();
		started = 1;
	} else {
		while (started == 0) {
			;
		}
		__sync_synchronize();
		printf("hart %d is starting\n", cpuid());
		trapinithart(); // install kernel trap vector
		timerInit();
		// kvminithart();    // turn on paging
		// plicinithart();   // ask PLIC for device interrupts
	}

	while (1) {
		;
	}
	// scheduler();
}
