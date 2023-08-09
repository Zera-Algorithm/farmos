#include "dev/sbi.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

void main();

void start_trap() {
	while (1);
}

// entry.S needs one stack per CPU.
// Note：内核栈只占一页的大小，所以不要放太大的数据结构
__attribute__((aligned(16))) char stack0[KSTACKSIZE * NCPU];

// entry.S jumps here in machine mode on stack0.
void start(long hartid, uint64 _dtb_entry) {
	// 设置dtb_entry
	extern uint64 dtbEntry;
	if (dtbEntry == 0) {
		dtbEntry = _dtb_entry;
	}

	// Supervisor: disable paging for now.
	w_satp(0);

	// enable some supervisor interrupt
	// w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
	w_sie(r_sie() | SIE_SEIE | SIE_STIE); // 不启用核间中断

	// 在每个CPU的tp寄存器中保存hartid
	w_tp(hartid);
	w_stvec((u64)start_trap);

	main();
}
