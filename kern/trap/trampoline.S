/**
 * @brief 此文件是用户态的异常入口
 *        应置于所有用户程序的虚拟内存中
 * @comment trampoline译为蹦床，意思是用户态到内核态的蹦床
 *
*/

// 引入专属于asm的头文件
// 因为汇编器理解不了部分C代码，如结构体等
#include <asm/trapframe.h>
#include <riscv.h>
#include <mm/memlayout.h>

.section trampSec
.globl trampoline
trampoline:
// 按4个字节的倍数对齐（TODO：trampoline按页对齐）
.align 4
.globl userVec
userVec:
	// trap.c会将stvec指向这里，所有所有用户态的trap都从这里开始
	// 注意：此时此时处于S态，但仍然是用户的页表

	// 保存a0
	csrw sscratch, a0

	// 加载TRAPFRAME
	// 每个用户都会有一个固定的虚拟地址用于存陷入异常时的寄存器状态
	li a0, TRAPFRAME

	// 在TRAPFRAME里面保存寄存器信息(除了a0)
	sd  ra,  OFFSET_RA(a0)
	sd  sp,  OFFSET_SP(a0)
	sd  gp,  OFFSET_GP(a0)
	sd  tp,  OFFSET_TP(a0)
	sd  t0,  OFFSET_T0(a0)
	sd  t1,  OFFSET_T1(a0)
	sd  t2,  OFFSET_T2(a0)
	sd  s0,  OFFSET_S0(a0)
	sd  s1,  OFFSET_S1(a0)
	sd  a1,  OFFSET_A1(a0)
	sd  a2,  OFFSET_A2(a0)
	sd  a3,  OFFSET_A3(a0)
	sd  a4,  OFFSET_A4(a0)
	sd  a5,  OFFSET_A5(a0)
	sd  a6,  OFFSET_A6(a0)
	sd  a7,  OFFSET_A7(a0)
	sd  s2,  OFFSET_S2(a0)
	sd  s3,  OFFSET_S3(a0)
	sd  s4,  OFFSET_S4(a0)
	sd  s5,  OFFSET_S5(a0)
	sd  s6,  OFFSET_S6(a0)
	sd  s7,  OFFSET_S7(a0)
	sd  s8,  OFFSET_S8(a0)
	sd  s9,  OFFSET_S9(a0)
	sd  s10, OFFSET_S10(a0)
	sd  s11, OFFSET_S11(a0)
	sd  t3,  OFFSET_T3(a0)
	sd  t4,  OFFSET_T4(a0)
	sd  t5,  OFFSET_T5(a0)
	sd  t6,  OFFSET_T6(a0)

	// 保存a0
	csrr t0, sscratch
	sd t0, OFFSET_A0(a0)

	// tp加载当前的hartid
	ld tp, OFFSET_HARTID(a0)

	// 保存epc
	csrr t0, sepc
	sd t0, OFFSET_EPC(a0)

	// t0 加载trap_handler
	ld t0, OFFSET_TRAP_HANDLER(a0)

	// t1 加载satp
	ld t1, OFFSET_KERNEL_SATP(a0)

	// 准备一个正确的sp
	ld sp, OFFSET_KERNEL_SP(a0)

	// 等待过去的内存访问操作结束
	sfence.vma zero, zero

	// 加载内核页表
	csrw satp, t1

	// 刷新MMU
	sfence.vma zero, zero

	jr t0


.align 4
.globl userRet
userRet:
	# userRet(pagetable)：a0保存页目录
	# 作为跳板，从内核态跳到用户态
	sfence.vma zero, zero
	csrw satp, a0
	sfence.vma zero, zero

	li a0, TRAPFRAME

	# 从TRAPFRAME恢复所有变量，除了a0
	ld  ra,  OFFSET_RA(a0)
	ld  sp,  OFFSET_SP(a0)
	ld  gp,  OFFSET_GP(a0)
	ld  tp,  OFFSET_TP(a0)
	ld  t0,  OFFSET_T0(a0)
	ld  t1,  OFFSET_T1(a0)
	ld  t2,  OFFSET_T2(a0)
	ld  s0,  OFFSET_S0(a0)
	ld  s1,  OFFSET_S1(a0)
	ld  a1,  OFFSET_A1(a0)
	ld  a2,  OFFSET_A2(a0)
	ld  a3,  OFFSET_A3(a0)
	ld  a4,  OFFSET_A4(a0)
	ld  a5,  OFFSET_A5(a0)
	ld  a6,  OFFSET_A6(a0)
	ld  a7,  OFFSET_A7(a0)
	ld  s2,  OFFSET_S2(a0)
	ld  s3,  OFFSET_S3(a0)
	ld  s4,  OFFSET_S4(a0)
	ld  s5,  OFFSET_S5(a0)
	ld  s6,  OFFSET_S6(a0)
	ld  s7,  OFFSET_S7(a0)
	ld  s8,  OFFSET_S8(a0)
	ld  s9,  OFFSET_S9(a0)
	ld  s10, OFFSET_S10(a0)
	ld  s11, OFFSET_S11(a0)
	ld  t3,  OFFSET_T3(a0)
	ld  t4,  OFFSET_T4(a0)
	ld  t5,  OFFSET_T5(a0)
	ld  t6,  OFFSET_T6(a0)

	# 恢复a0
	ld  a0,  OFFSET_A0(a0)

	# 利用userTrapRet设置的sstatus和sepc，返回用户态
	sret

