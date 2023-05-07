#include "SBI.h"
#include "defs.h"
#include "dtb.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

#define INTERVAL 5000000

// 获取当前的时间（以Cycles为单位）
uint64 getTime() {
	uint64 n;
	asm volatile("rdtime %0" : "=r"(n));
	return n;
}

void timerInit() {
	intr_on();
	SBI_SET_TIMER(getTime() + INTERVAL);
}

void timerSetNextTick() {
	SBI_SET_TIMER(getTime() + INTERVAL);
}