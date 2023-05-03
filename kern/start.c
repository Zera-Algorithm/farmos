#include "SBI.h"
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__((aligned(16))) char stack0[4096 * NCPU];

// a scratch area per CPU for machine-mode timer interrupts.
uint64 timer_scratch[NCPU][5];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// // 测试SBI中的内联汇编是否能正常工作
// void test() {
//   struct sbiret ret;
//   ret = SBI_RFENCE_SFENCE_VMA_ASID(1,2,3,4,5);
//   printf("%lx, %lx\n", ret.error, ret.value);
// }

// entry.S jumps here in machine mode on stack0.
void start(long hartid, uint64 _dtb_entry) {
	// 设置dtb_entry
	extern uint64 dtb_entry;
	if (hartid == 0) {
		dtb_entry = _dtb_entry;
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

	// keep each CPU's hartid in its tp register, for cpuid().
	w_tp(hartid);

	// 测试SBI的功能
	SBI_PUTCHAR('H');
	SBI_PUTCHAR('e');
	SBI_PUTCHAR('l');
	SBI_PUTCHAR('l');
	SBI_PUTCHAR('o');
	SBI_PUTCHAR('\n');

	main();
}

// arrange to receive timer interrupts.
// they will arrive in machine mode at
// at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void timerinit() {
	// each CPU has a separate source of timer interrupts.
	int id = r_mhartid();

	// ask the CLINT for a timer interrupt.
	int interval = 1000000; // cycles; about 1/10th second in qemu.
	*(uint64 *)CLINT_MTIMECMP(id) = *(uint64 *)CLINT_MTIME + interval;

	// prepare information in scratch[] for timervec.
	// scratch[0..2] : space for timervec to save registers.
	// scratch[3] : address of CLINT MTIMECMP register.
	// scratch[4] : desired interval (in cycles) between timer interrupts.
	uint64 *scratch = &timer_scratch[id][0];
	scratch[3] = CLINT_MTIMECMP(id);
	scratch[4] = interval;
	w_mscratch((uint64)scratch);

	// set the machine-mode trap handler.
	w_mtvec((uint64)timervec);

	// enable machine-mode interrupts.
	w_mstatus(r_mstatus() | MSTATUS_MIE);

	// enable machine-mode timer interrupts.
	w_mie(r_mie() | MIE_MTIE);
}
