#ifndef _TRAPFRAME_H
#define _TRAPFRAME_H

#include <types.h>

typedef struct trapframe {
	u64 kernel_satp;  // 保存内核页表
	u64 trap_handler; // 内核态异常针对用户异常的处理函数（C函数）
	u64 epc;	  // 用户的epc
	u64 hartid;	  // 当前的hartid，取自tp寄存器
	u64 ra;
	u64 sp;
	u64 gp;
	u64 tp;
	u64 t0;
	u64 t1;
	u64 t2;
	u64 s0;
	u64 s1;
	u64 a0;
	u64 a1;
	u64 a2;
	u64 a3;
	u64 a4;
	u64 a5;
	u64 a6;
	u64 a7;
	u64 s2;
	u64 s3;
	u64 s4;
	u64 s5;
	u64 s6;
	u64 s7;
	u64 s8;
	u64 s9;
	u64 s10;
	u64 s11;
	u64 t3;
	u64 t4;
	u64 t5;
	u64 t6;
	u64 kernel_sp; // 内核的sp指针
	u64 ft0;
	u64 ft1;
	u64 ft2;
	u64 ft3;
	u64 ft4;
	u64 ft5;
	u64 ft6;
	u64 ft7;
	u64 fs0;
	u64 fs1;
	u64 fa0;
	u64 fa1;
	u64 fa2;
	u64 fa3;
	u64 fa4;
	u64 fa5;
	u64 fa6;
	u64 fa7;
	u64 fs2;
	u64 fs3;
	u64 fs4;
	u64 fs5;
	u64 fs6;
	u64 fs7;
	u64 fs8;
	u64 fs9;
	u64 fs10;
	u64 fs11;
	u64 ft8;
	u64 ft9;
	u64 ft10;
	u64 ft11;
} trapframe_t;

typedef struct ktrapframe {
	u64 ra;
	u64 sp;
	u64 gp;
	u64 tp;
	u64 t0;
	u64 t1;
	u64 t2;
	u64 s0;
	u64 s1;
	u64 a0;
	u64 a1;
	u64 a2;
	u64 a3;
	u64 a4;
	u64 a5;
	u64 a6;
	u64 a7;
	u64 s2;
	u64 s3;
	u64 s4;
	u64 s5;
	u64 s6;
	u64 s7;
	u64 s8;
	u64 s9;
	u64 s10;
	u64 s11;
	u64 t3;
	u64 t4;
	u64 t5;
	u64 t6;
} ktrapframe_t;

// 注意：Trapframe 页必须能容纳所有核心的 Trapframe
// 即 TRAPFRAME_SIZE * NCPU <= PAGE_SIZE
#define TRAPFRAME_SIZE sizeof(trapframe_t)

#endif // _TRAPFRAME_H