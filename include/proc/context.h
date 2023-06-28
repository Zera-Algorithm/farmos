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

#endif // _CONTEXT_H_