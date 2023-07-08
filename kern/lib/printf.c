#include <dev/sbi.h>
#include <lib/printf.h>
#include <lib/terminal.h>
#include <lib/vprint.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <riscv.h>

// 建立一个printf的锁，保证同一个printf中的数据都能在一次输出完毕
mutex_t pr_lock;

void printInit() {
	mtx_init(&pr_lock, "printf", false, MTX_SPIN); // 此处禁止调试信息输出！否则会递归获取锁
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

	mtx_lock(&pr_lock); // todo lock
	vprintfmt(output, NULL, fmt, ap);
	mtx_unlock(&pr_lock);

	va_end(ap);
}

void sprintf(char *buf, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char *mybuf = buf;

	mtx_lock(&pr_lock);
	vprintfmt(outputToStr, &mybuf, fmt, ap);
	mtx_unlock(&pr_lock);

	va_end(ap);
}

// TODO: 加上时间戳、CPU编号等信息
void _log(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	mtx_lock(&pr_lock);
	// 输出日志头
	printfNoLock("%s %2d %12s:%-4d %12s()" SGR_RESET ": ",
		     FARM_INFO "[INFO]" SGR_RESET SGR_BLUE, cpu_this_id(), file, line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	mtx_unlock(&pr_lock);

	va_end(ap);
}

void _warn(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	mtx_lock(&pr_lock);
	// 输出日志头
	printfNoLock("%s %2d %12s:%-4d %12s()" SGR_RESET ": ",
		     FARM_WARN "[WARN]" SGR_RESET SGR_YELLOW, cpu_this_id(), file, line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	mtx_unlock(&pr_lock);

	va_end(ap);
}

void _error(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	mtx_lock(&pr_lock);
	// 输出日志头
	printfNoLock("%s %2d %12s:%-4d %12s()" SGR_RESET ": ",
		     FARM_ERROR "[ERROR]" SGR_RESET SGR_RED, cpu_this_id(), file, line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	printfNoLock("\n\n");
	mtx_unlock(&pr_lock);

	va_end(ap);

	SBI_SYSTEM_RESET(0, 0);

	while (1)
		;
}

/**
 * @brief 在发生异常时，打印寄存器的信息
 */
void printReg(struct trapframe *tf) {
	printf("ra  = 0x%016lx\t", tf->ra);
	printf("sp  = 0x%016lx\t", tf->sp);
	printf("gp  = 0x%016lx\n", tf->gp);
	printf("tp  = 0x%016lx\t", tf->tp);
	printf("t0  = 0x%016lx\t", tf->t0);
	printf("t1  = 0x%016lx\n", tf->t1);
	printf("t2  = 0x%016lx\t", tf->t2);
	printf("s0  = 0x%016lx\t", tf->s0);
	printf("s1  = 0x%016lx\n", tf->s1);
	printf("a0  = 0x%016lx\t", tf->a0);
	printf("a1  = 0x%016lx\t", tf->a1);
	printf("a2  = 0x%016lx\n", tf->a2);
	printf("a3  = 0x%016lx\t", tf->a3);
	printf("a4  = 0x%016lx\t", tf->a4);
	printf("a5  = 0x%016lx\n", tf->a5);
	printf("a6  = 0x%016lx\t", tf->a6);
	printf("a7  = 0x%016lx\t", tf->a7);
	printf("s2  = 0x%016lx\n", tf->s2);
	printf("s3  = 0x%016lx\t", tf->s3);
	printf("s4  = 0x%016lx\t", tf->s4);
	printf("s5  = 0x%016lx\n", tf->s5);
	printf("s6  = 0x%016lx\t", tf->s6);
	printf("s7  = 0x%016lx\t", tf->s7);
	printf("s8  = 0x%016lx\n", tf->s8);
	printf("s9  = 0x%016lx\t", tf->s9);
	printf("s10 = 0x%016lx\t", tf->s10);
	printf("s11 = 0x%016lx\n", tf->s11);
	printf("t3  = 0x%016lx\t", tf->t3);
	printf("t4  = 0x%016lx\t", tf->t4);
	printf("t5  = 0x%016lx\n", tf->t5);
	printf("t6  = 0x%016lx\n", tf->t6);
}
