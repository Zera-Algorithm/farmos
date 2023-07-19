#include <dev/sbi.h>
#include <proc/cpu.h>
#include <riscv.h>

struct cpu cpus[NCPU];

/**
 * @brief 获取当前CPU的ID，调用时必须在关中断的情况下
 */
register_t cpu_this_id() {
	return r_tp();
}

/**
 * @brief 获取当前CPU的结构体，调用时必须在关中断的情况下
 */
cpu_t *cpu_this() {
	return &cpus[cpu_this_id()];
}

void cpu_idle() {
	intr_on();
	for (int i = 0; i < 1000000; i++)
		;
	intr_off();
}

void cpu_halt() {
	intr_off();
	SBI_SYSTEM_RESET(0, 0);
	// 关机后不会执行到这里
	while (1)
		;
}

bool cpu_allidle() {
	for (int i = 0; i < NCPU; i++) {
		if (!cpus[i].cpu_idle) {
			return false;
		}
	}
	return true;
}