#include "lib/printf.h"
#include "defs.h"
#include "dev/sbi.h"
#include "fs/file.h"
#include "fs/fs.h"
#include "lib/print.h"
#include "lock/sleeplock.h"
#include "lock/spinlock.h"
#include "mm/memlayout.h"
#include "proc/proc.h"
#include "riscv.h"
#include "types.h"

// 建立一个printf的锁，保证同一个printf中的数据都能在一次输出完毕
struct spinlock pr_lock;

void output(void *data, const char *buf, size_t len) {
	for (int i = 0; i < len; i++) {
		SBI_PUTCHAR(buf[i]);
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

void printfNoLock(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	vprintfmt(output, NULL, fmt, ap);
	va_end(ap);
}

void printfinit() {
	initlock(&pr_lock, "printf");
}
