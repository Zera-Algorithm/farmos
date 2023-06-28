#ifndef _ASM_TRAPFRAME_H
#define _ASM_TRAPFRAME_H
/**
 * @brief 下面定义Trapframe各个字段的偏移
 */
#define OFFSET_KERNEL_SATP 0
#define OFFSET_TRAP_HANDLER 8
#define OFFSET_EPC 16
#define OFFSET_HARTID 24
#define OFFSET_RA 32
#define OFFSET_SP 40
#define OFFSET_GP 48
#define OFFSET_TP 56
#define OFFSET_T0 64
#define OFFSET_T1 72
#define OFFSET_T2 80
#define OFFSET_S0 88
#define OFFSET_S1 96
#define OFFSET_A0 104
#define OFFSET_A1 112
#define OFFSET_A2 120
#define OFFSET_A3 128
#define OFFSET_A4 136
#define OFFSET_A5 144
#define OFFSET_A6 152
#define OFFSET_A7 160
#define OFFSET_S2 168
#define OFFSET_S3 176
#define OFFSET_S4 184
#define OFFSET_S5 192
#define OFFSET_S6 200
#define OFFSET_S7 208
#define OFFSET_S8 216
#define OFFSET_S9 224
#define OFFSET_S10 232
#define OFFSET_S11 240
#define OFFSET_T3 248
#define OFFSET_T4 256
#define OFFSET_T5 264
#define OFFSET_T6 272
#define OFFSET_KERNEL_SP 280

#define CTX_RA_OFF 0
#define CTX_SP_OFF 8
#define CTX_GP_OFF 16
#define CTX_TP_OFF 24
#define CTX_S0_OFF 32
#define CTX_S1_OFF 40
#define CTX_S2_OFF 48
#define CTX_S3_OFF 56
#define CTX_S4_OFF 64
#define CTX_S5_OFF 72
#define CTX_S6_OFF 80
#define CTX_S7_OFF 88
#define CTX_S8_OFF 96
#define CTX_S9_OFF 104
#define CTX_S10_OFF 112
#define CTX_S11_OFF 120
#endif