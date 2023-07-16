#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <types.h>

/**
 * 被调用者保存的寄存器上下文
 */
typedef struct context {
	u64 ctx_ra;
	u64 ctx_sp;
	u64 ctx_gp;
	u64 ctx_tp;
	u64 ctx_s[12];
} context_t;

// 汇编函数（switch.S）
void ctx_switch(context_t *oldtd, register_t param);
void ctx_enter(context_t *inittd) __attribute__((noreturn));

#endif // _CONTEXT_H_