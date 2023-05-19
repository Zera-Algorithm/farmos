#include <dev/sbi.h>
#include <lib/string.h>
#include <mm/memory.h>
#include <proc/proc.h>

// long类型的最大数字
#define MAXLONG 0xFFFFFFFFFFFFFFFFul

/**
 * @brief 刷新TLB
 * @return 此函数只能成功不能失败
 */
void flushTlb() {
	// 调用SBI，命令所有核都执行tlb刷新命令
	SBI_RFENCE_SFENCE_VMA((1 << NCPU) - 1, 0, 0, MAXLONG);
}
