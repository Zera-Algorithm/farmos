#include <dev/sbi.h>
#include <lib/printf.h>
#include <lib/log.h>
#include <mm/mmu.h>
#include <param.h>
#include <riscv.h>

// long类型的最大数字
#define MAXLONG 0xFFFFFFFFFFFFFFFFul

/**
 * @brief 通知所有核心刷新TLB
 * @return 此函数只能成功不能失败
 */
void tlbFlush() {
	// 调用SBI，命令所有核都执行tlb刷新命令
	// todo 带参数优化
	struct sbiret ret = SBI_RFENCE_SFENCE_VMA((1 << NCPU) - 1, 0, 0, MAXLONG);
	if (ret.error) {
		panic("tlbFlush: SBI_RFENCE_SFENCE_VMA failed: %d, value = %d\n", ret.error, ret.value);
	}
}

void vmEnable() {
	// // 等待之前对页表的写操作结束
	// sfence_vma();
	extern Pte *kernPd;
	w_satp(MAKE_SATP(kernPd));

	// 刷新TLB（单核）
	sfence_vma();
}

Pte *ptFetch() {
	return ((Pte *)((r_satp() & ((1ul << 44) - 1)) << 12));
}
