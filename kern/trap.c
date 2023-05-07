#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "printf.h"
#include "SBI.h"

extern void kernelvec();

// 内核态中断入口
void kerneltrap() {
    printf("trap on CPU %d!\n", cpuid());
    // TODO: 实现更丰富的中断处理
    timerSetNextTick();
}

// 设置异常处理跳转入口
void trapinithart(void) {
  w_stvec((uint64)kernelvec);
}