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


// 该段在用户态和内核态均有映射，因此不应该使用内核地址或用户地址
// 只能使用两端均映射的地址，也就是 TRAMPOLINE 和 TRAPFRAME
.section trampSec
.globl trampoline
trampoline:
// 按4个字节的倍数对齐（trampoline按页对齐）
.align 4
.globl userVec
userVec:
	// e0: 硬件设置好 sepc/scause/stval，屏蔽中断（压入 SPIE 位），进入监管态

	// e1: 从 sscratch 获取 TRAPFRAME 地址，与 a0 寄存器交换
	csrrw a0, sscratch, a0
	// e1: 此时 a0 是本核心的 TRAPFRAME 地址，将所有 31 个寄存器保存到 TRAPFRAME
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


	// 保存浮点寄存器
	fsd ft0, OFFSET_FT0(a0)
	fsd ft1, OFFSET_FT1(a0)
	fsd ft2, OFFSET_FT2(a0)
	fsd ft3, OFFSET_FT3(a0)
	fsd ft4, OFFSET_FT4(a0)
	fsd ft5, OFFSET_FT5(a0)
	fsd ft6, OFFSET_FT6(a0)
	fsd ft7, OFFSET_FT7(a0)
	fsd fs0, OFFSET_FS0(a0)
	fsd fs1, OFFSET_FS1(a0)
	fsd fa0, OFFSET_FA0(a0)
	fsd fa1, OFFSET_FA1(a0)
	fsd fa2, OFFSET_FA2(a0)
	fsd fa3, OFFSET_FA3(a0)
	fsd fa4, OFFSET_FA4(a0)
	fsd fa5, OFFSET_FA5(a0)
	fsd fa6, OFFSET_FA6(a0)
	fsd fa7, OFFSET_FA7(a0)
	fsd fs2, OFFSET_FS2(a0)
	fsd fs3, OFFSET_FS3(a0)
	fsd fs4, OFFSET_FS4(a0)
	fsd fs5, OFFSET_FS5(a0)
	fsd fs6, OFFSET_FS6(a0)
	fsd fs7, OFFSET_FS7(a0)
	fsd fs8, OFFSET_FS8(a0)
	fsd fs9, OFFSET_FS9(a0)
	fsd fs10, OFFSET_FS10(a0)
	fsd fs11, OFFSET_FS11(a0)
	fsd ft8, OFFSET_FT8(a0)
	fsd ft9, OFFSET_FT9(a0)
	fsd ft10, OFFSET_FT10(a0)
	fsd ft11, OFFSET_FT11(a0)


	csrr t0, sscratch
	sd t0, OFFSET_A0(a0)

	// e2: 保存 sepc 到 TRAPFRAME
	csrr t0, sepc
	sd t0, OFFSET_EPC(a0)

	// e3: 准备内核线程现场（入口地址、内核栈）
	ld t0, OFFSET_TRAP_HANDLER(a0)
	ld sp, OFFSET_KERNEL_SP(a0)

	// e4: 恢复内核号（切到用户态时暂存在 TRAPFRAME 里）
	ld tp, OFFSET_HARTID(a0)

	// e5: 内存屏障、切换到内核页表、再次内存屏障
	ld t1, OFFSET_KERNEL_SATP(a0)
	sfence.vma zero, zero
	csrw satp, t1
	sfence.vma zero, zero

	// e6. 刷新代码Cache
	fence.i

	jr t0


.align 4
.globl userRet
/*
 * void userRet(u64 hart_tf, u64 user_satp)
 * a0/x10 = user_satp
 */
userRet:

	// ue5: 内存屏障、切换到用户页表、再次内存屏障
	sfence.vma zero, zero
	csrw satp, a1
	sfence.vma zero, zero

	// ue4: 由内核函数存储内核号
	// ue3: 由内核函数保存内核线程现场（TRAPFRAME->KERNEL_SP/TRAPFRAME->TRAP_HANDLER）

	// ue2: 加载用户 a0 寄存器到 sscratch 从 TRAPFRAME 中恢复 sepc
	ld t0, OFFSET_A0(a0)
    csrw sscratch, t0

	ld t0, OFFSET_EPC(a0)
	csrw sepc, t0

	// ue1: 将所有 31 个寄存器从 TRAPFRAME 中恢复
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


	// 从 TRAPFRAME 中恢复浮点寄存器
	fld ft0, OFFSET_FT0(a0)
	fld ft1, OFFSET_FT1(a0)
	fld ft2, OFFSET_FT2(a0)
	fld ft3, OFFSET_FT3(a0)
	fld ft4, OFFSET_FT4(a0)
	fld ft5, OFFSET_FT5(a0)
	fld ft6, OFFSET_FT6(a0)
	fld ft7, OFFSET_FT7(a0)
	fld fs0, OFFSET_FS0(a0)
	fld fs1, OFFSET_FS1(a0)
	fld fa0, OFFSET_FA0(a0)
	fld fa1, OFFSET_FA1(a0)
	fld fa2, OFFSET_FA2(a0)
	fld fa3, OFFSET_FA3(a0)
	fld fa4, OFFSET_FA4(a0)
	fld fa5, OFFSET_FA5(a0)
	fld fa6, OFFSET_FA6(a0)
	fld fa7, OFFSET_FA7(a0)
	fld fs2, OFFSET_FS2(a0)
	fld fs3, OFFSET_FS3(a0)
	fld fs4, OFFSET_FS4(a0)
	fld fs5, OFFSET_FS5(a0)
	fld fs6, OFFSET_FS6(a0)
	fld fs7, OFFSET_FS7(a0)
	fld fs8, OFFSET_FS8(a0)
	fld fs9, OFFSET_FS9(a0)
	fld fs10, OFFSET_FS10(a0)
	fld fs11, OFFSET_FS11(a0)
	fld ft8, OFFSET_FT8(a0)
	fld ft9, OFFSET_FT9(a0)
	fld ft10, OFFSET_FT10(a0)
	fld ft11, OFFSET_FT11(a0)


	csrrw a0, sscratch, a0

	// 刷新代码缓存
	fence.i

	// ue0: 硬件从 sepc 恢复，开启中断（恢复 SPIE 位），进入用户态
	sret

