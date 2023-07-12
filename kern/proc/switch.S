#include <mm/memlayout.h>
#include <asm/trapframe.h>
/*
 * void td_switch(thread_t *old)
 *
 * a0/x10 = old
 * x3 to x7, x16 and x17 are caller saved
 */

.globl sched_switch
.globl td_switch
td_switch:
        // 保存旧线程的上下文
        sd ra, CTX_RA_OFF(a0)
        sd sp, CTX_SP_OFF(a0)
        
        sd s0, CTX_S0_OFF(a0)
        sd s1, CTX_S1_OFF(a0)
        sd s2, CTX_S2_OFF(a0)
        sd s3, CTX_S3_OFF(a0)
        sd s4, CTX_S4_OFF(a0)
        sd s5, CTX_S5_OFF(a0)
        sd s6, CTX_S6_OFF(a0)
        sd s7, CTX_S7_OFF(a0)
        sd s8, CTX_S8_OFF(a0)
        sd s9, CTX_S9_OFF(a0)
        sd s10, CTX_S10_OFF(a0)
        sd s11, CTX_S11_OFF(a0)

        // 切换到原生内核栈
        la sp, stack0           // sp = stack0
        li t0, KSTACKSIZE       // t0 = KSTACKSIZE
        mv t1, tp               // t1 = hartid
        addi t1, t1, 1          // t1 = hartid + 1
        mul t0, t0, t1          // t0 = KSTACKSIZE * (hartid + 1)
        add sp, sp, t0          // sp = stack0 + KSTACKSIZE * (hartid + 1)
        addi sp, sp, -16          // sp = stack0 + KSTACKSIZE * (hartid + 1) - 16

        // 此时旧进程的上下文已经保存完毕，可以认为是无锁临界区
        call sched_switch
        // 函数返回了新进程的指针，保存到 a0 中

        // 恢复新线程的上下文
        ld s0, CTX_S0_OFF(a0)
        ld s1, CTX_S1_OFF(a0)
        ld s2, CTX_S2_OFF(a0)
        ld s3, CTX_S3_OFF(a0)
        ld s4, CTX_S4_OFF(a0)
        ld s5, CTX_S5_OFF(a0)
        ld s6, CTX_S6_OFF(a0)
        ld s7, CTX_S7_OFF(a0)
        ld s8, CTX_S8_OFF(a0)
        ld s9, CTX_S9_OFF(a0)
        ld s10, CTX_S10_OFF(a0)
        ld s11, CTX_S11_OFF(a0)

        ld ra, CTX_RA_OFF(a0)
        ld sp, CTX_SP_OFF(a0)
       
        ret

	
.globl td_initentry
td_initentry:
        // 加载第一个线程的的上下文
        ld s0, CTX_S0_OFF(a0)
        ld s1, CTX_S1_OFF(a0)
        ld s2, CTX_S2_OFF(a0)
        ld s3, CTX_S3_OFF(a0)
        ld s4, CTX_S4_OFF(a0)
        ld s5, CTX_S5_OFF(a0)
        ld s6, CTX_S6_OFF(a0)
        ld s7, CTX_S7_OFF(a0)
        ld s8, CTX_S8_OFF(a0)
        ld s9, CTX_S9_OFF(a0)
        ld s10, CTX_S10_OFF(a0)
        ld s11, CTX_S11_OFF(a0)

        ld ra, CTX_RA_OFF(a0)
        ld sp, CTX_SP_OFF(a0)
       
        ret