#ifndef _WAIT_H
#define _WAIT_H
// 表示状态信息的位域
#include <types.h>
// 联合
union WaitStatus {
	u32 val; // 整体值

	// 从低地址到高地址
	struct {
		unsigned low8 : 8;
		unsigned high8 : 8;
		unsigned __empty : 16;
	} __attribute__((packed)) bits; // 取消优化对齐
};

// 以下是判断进程状态的一些宏
#define WIFEXITED(status) ((union WaitStatus)(status).bits.low8 == 0)
#define WIFSIGNALED(status)                                                                        \
	(((signed char)(((union WaitStatus)(status).bits.low8 & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(status) ((union WaitStatus)(status).bits.low8 == 0x7f)
#define WIFCONTINUED(status)                                                                       \
	((union WaitStatus)(status).bits.low8 == 0xff &&                                           \
	 (union WaitStatus)(status).bits.high8 == 0xff)

// wait系统调用的options掩码
#define WNOHANG 1    /* Don't block waiting.  */
#define WUNTRACED 2  /* Report status of stopped children.  */
#define WCONTINUED 8 /* Report continued child.  */

u64 wait(struct Proc *proc, i64 pid, u64 pStatus, int options);
void tryWakeupParentProc(struct Proc *child);
#endif