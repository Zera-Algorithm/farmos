#include "printf.h"
#include "SBI.h"
#include "defs.h"
#include "file.h"
#include "fs.h"
#include "memlayout.h"
#include "param.h"
#include "print.h"
#include "proc.h"
#include "riscv.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "types.h"

// 建立一个printf的锁，保证同一个printf中的数据都能在一次输出完毕
struct spinlock pr_lock;

void printcharc(char ch) {
	SBI_PUTCHAR(ch);
}

void output(void *data, const char *buf, size_t len) {
	for (int i = 0; i < len; i++) {
		printcharc(buf[i]);
	}
}

void printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	acquire(&pr_lock);

	vprintfmt(output, NULL, fmt, ap);
	va_end(ap);

	release(&pr_lock);
}

void printfinit() {
	initlock(&pr_lock, "printf");
}
