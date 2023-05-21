#include <dev/sbi.h>
#include <lib/string.h>
#include <mm/memory.h>
#include <proc/proc.h>

// long类型的最大数字
#define MAXLONG 0xFFFFFFFFFFFFFFFFul

/**
 * @brief 刷新TLB
 * TODO：根据curPgdir判断是否应当刷新TLB
 * @return 此函数只能成功不能失败
 */
void flushTlb() {
	// 调用SBI，命令所有核都执行tlb刷新命令
	SBI_RFENCE_SFENCE_VMA((1 << NCPU) - 1, 0, 0, MAXLONG);
}

void enablePagingHart() {
	// // 等待之前对页表的写操作结束
	// sfence_vma();
	extern Pte *kernPd;
	w_satp(MAKE_SATP(kernPd));

	// 刷新TLB（单核）
	sfence_vma();
}

Pte *curPgdir() {
	warn("Deprecated function!\n");
	return ((Pte *)CUR_PGDIR);
}
