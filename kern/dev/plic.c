#include <mm/memlayout.h>
#include <proc/proc.h>
#include <types.h>

//
// the riscv Platform Level Interrupt Controller (PLIC)
// RISCV平台级中断控制器
//

void plicInit() {
	// 开启VIRTIO的中断
	// OpenSBI默认会将大部分中断和异常委托给S模式

	// *(uint32*)(PLIC + UART0_IRQ*4) = 1; // 暂时不开启UART的中断
	*(uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;
}

void plicInitHart() {
	int hart = cpuid();

	// 允许VIRTIO的IRQ中断
	*(u32 *)PLIC_SENABLE(hart) = (1 << VIRTIO0_IRQ);

	// 设置优先级为0
	*(u32 *)PLIC_SPRIORITY(hart) = 0;
}

/**
 * @brief 向PLIC索要当前中断的IRQ编号
 */
int plicClaim() {
	int hart = cpuid();
	int irq = *(u32 *)PLIC_SCLAIM(hart);
	return irq;
}

/**
 * @brief 告知plic我们已经处理完了irq代指的中断
 * @param irq 处理完的中断编号
 */
void plicComplete(int irq) {
	int hart = cpuid();
	*(u32 *)PLIC_SCLAIM(hart) = irq;
}
