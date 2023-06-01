#ifndef _printk_h_
#define _printk_h_

#include <proc/proc.h>
#include <stdarg.h>

// 初始化锁
void printInit(void);

// 正常输出（带锁）
void printf(const char *fmt, ...);

// 输出到字符串
void sprintf(char *buf, const char *fmt, ...);

void printReg(struct trapframe *tf);

#endif /* _printk_h_ */
