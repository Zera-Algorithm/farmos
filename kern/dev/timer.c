#include "dev/dtb.h"
#include "dev/sbi.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

#define INTERVAL 5000000

/**
 * @brief 获取当前的时间（以Cycles为单位）
 * 		  RISCV的核内时钟发生器的频率为10^7Hz
 * @returns 设备启动到现在总共运行的时钟数
 */
uint64 getTime() {
	uint64 n;
	asm volatile("rdtime %0" : "=r"(n));
	return n;
}

/**
 * @brief 打开全局中断，设置核内时钟下一Tick的时间，以初始化时钟
 */
void timerInit() {
	intr_on();
	SBI_SET_TIMER(getTime() + INTERVAL);
}

/**
 * @brief 用于在发生时钟中断时设置下一个Tick的时间
 */
void timerSetNextTick() {
	SBI_SET_TIMER(getTime() + INTERVAL);
}
