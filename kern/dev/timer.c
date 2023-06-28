#include "dev/dtb.h"
#include "dev/sbi.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include <dev/timer.h>

/**
 * @brief 获取当前的时间（以Cycles为单位）
 * 		  RISCV的核内时钟发生器的频率为10^7Hz
 * @returns 设备启动到现在总共运行的时钟数
 */
u64 getTime() {
	uint64 n;
	asm volatile("rdtime %0" : "=r"(n));
	return n;
}

/**
 * @brief 获取以微秒记的时间
 */
u64 getUSecs() {
	return getTime() / CLOCK_PER_USEC;
}

/**
 * @brief 打开全局中断，设置核内时钟下一Tick的时间，以初始化时钟
 */
void timerInit() {
	SBI_SET_TIMER(getTime() + INTERVAL);
}

/**
 * @brief 用于在发生时钟中断时设置下一个Tick的时间
 */
void timerSetNextTick() {
	SBI_SET_TIMER(getTime() + INTERVAL);
}
