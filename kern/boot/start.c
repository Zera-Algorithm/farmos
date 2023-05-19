#include "defs.h"
#include "dev/sbi.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

void main();

// entry.S needs one stack per CPU.
// Note：内核栈只占一页的大小，所以不要放太大的数据结构
__attribute__((aligned(16))) char stack0[4096 * NCPU];

// a scratch area per CPU for machine-mode timer interrupts.
uint64 timer_scratch[NCPU][5];

// // 测试SBI中的内联汇编是否能正常工作
// void test() {
//   struct sbiret ret;
//   ret = SBI_RFENCE_SFENCE_VMA_ASID(1,2,3,4,5);
//   printf("%lx, %lx\n", ret.error, ret.value);
// }

// entry.S jumps here in machine mode on stack0.
void start(long hartid, uint64 _dtb_entry) {
	// 设置dtb_entry
	extern uint64 dtbEntry;
	if (hartid == 0) {
		dtbEntry = _dtb_entry;
	}

	// Supervisor: disable paging for now.
	w_satp(0);

	// enable some supervisor interrupt
	w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

	// 在 Supervisor 下无效
	// // configure Physical Memory Protection to give supervisor mode
	// // access to all of physical memory.
	// w_pmpaddr0(0x3fffffffffffffull);
	// w_pmpcfg0(0xf);

	// // ask for clock interrupts.
	// timerinit();

	// 在每个CPU的tp寄存器中保存hartid
	w_tp(hartid);

	// // 测试SBI的功能
	// SBI_PUTCHAR('H');
	// SBI_PUTCHAR('e');
	// SBI_PUTCHAR('l');
	// SBI_PUTCHAR('l');
	// SBI_PUTCHAR('o');
	// SBI_PUTCHAR('\n');

	main();
}
