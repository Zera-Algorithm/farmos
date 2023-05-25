#include <dev/sbi.h>
#include <lib/print.h>
#include <lib/printf.h>
#include <lib/terminal.h>
#include <lock/spinlock.h>
#include <riscv.h>

// 建立一个printf的锁，保证同一个printf中的数据都能在一次输出完毕
struct spinlock pr_lock;

void printInit() {
	initlock(&pr_lock, "printf");
}

// vprintfmt只调用output输出可输出字符，不包括0，所以需要记得在字符串后补0
static void outputToStr(void *data, const char *buf, size_t len) {
	char **strBuf = (char **)data;
	for (int i = 0; i < len; i++) {
		(*strBuf)[i] = buf[i];
	}
	(*strBuf)[len] = 0;
	*strBuf += len;
}

static void output(void *data, const char *buf, size_t len) {
	for (int i = 0; i < len; i++) {
		SBI_PUTCHAR(buf[i]);
	}
}

static void printfNoLock(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintfmt(output, NULL, fmt, ap);
	va_end(ap);
}

void printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	acquire(&pr_lock);
	vprintfmt(output, NULL, fmt, ap);
	release(&pr_lock);

	va_end(ap);
}

void sprintf(char *buf, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char *mybuf = buf;

	acquire(&pr_lock);
	vprintfmt(outputToStr, &mybuf, fmt, ap);
	release(&pr_lock);

	va_end(ap);
}

void _log(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	acquire(&pr_lock);
	// 输出日志头
	printfNoLock("%s %12s:%-4d %12s()" SGR_RESET ": ", FARM_INFO "[INFO]" SGR_RESET SGR_BLUE,
		     file, line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	release(&pr_lock);

	va_end(ap);
}

void _warn(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	acquire(&pr_lock);
	// 输出日志头
	printfNoLock("%s %12s:%-4d %12s()" SGR_RESET ": ", FARM_WARN "[WARN]" SGR_RESET SGR_YELLOW,
		     file, line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	release(&pr_lock);

	va_end(ap);
}

void _error(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	acquire(&pr_lock);
	// 输出日志头
	printfNoLock("%s %12s:%-4d %12s()" SGR_RESET ": ", FARM_ERROR "[ERROR]" SGR_RESET SGR_RED,
		     file, line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	printfNoLock("\n\n");
	release(&pr_lock);

	va_end(ap);

	SBI_SYSTEM_RESET(0, 0);

	while (1)
		;
}
