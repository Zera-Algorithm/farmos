#ifndef _TRAPFRAME_H
#define _TRAPFRAME_H
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
#endif