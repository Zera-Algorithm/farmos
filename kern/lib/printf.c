#include <dev/sbi.h>
#include <lib/print.h>
#include <lib/printf.h>
#include <lib/terminal.h>
#include <lock/spinlock.h>

// 建立一个printf的锁，保证同一个printf中的数据都能在一次输出完毕
struct spinlock pr_lock;

void printInit() {
	initlock(&pr_lock, "printf");
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

void _panic(const char *file, int line, const char *func, const char *fmt, ...) {
	// uint64 sp, ra, badva, sr, cause, epc;
	// asm("move %0, $29" : "=r"(sp) :);
	// asm("move %0, $31" : "=r"(ra) :);
	// asm("mfc0 %0, $8" : "=r"(badva) :);
	// asm("mfc0 %0, $12" : "=r"(sr) :);
	// asm("mfc0 %0, $13" : "=r"(cause) :);
	// asm("mfc0 %0, $14" : "=r"(epc) :);

	printf("panic at %s:%d (%s): ", file, line, func);

	va_list ap;
	va_start(ap, fmt);
	vprintfmt(output, NULL, fmt, ap);
	va_end(ap);

	// 	printk("\n"
	// 	       "ra:    %08x  sp:  %08x  Status: %08x\n"
	// 	       "Cause: %08x  EPC: %08x  BadVA:  %08x\n",
	// 	       ra, sp, sr, cause, epc, badva);

	// #if !defined(LAB) || LAB >= 3
	// 	extern struct Env envs[];
	// 	extern struct Env *curenv;
	// 	extern struct Pde *cur_pgdir;

	// 	if ((u_long)curenv >= KERNBASE) {
	// 		printk("curenv:    %x (id = 0x%x, off = %d)\n", curenv, curenv->env_id,
	// 		       curenv - envs);
	// 	} else if (curenv) {
	// 		printk("curenv:    %x (invalid)\n", curenv);
	// 	} else {
	// 		printk("curenv:    NULL\n");
	// 	}

	// 	if ((u_long)cur_pgdir >= KERNBASE) {
	// 		printk("cur_pgdir: %x\n", cur_pgdir);
	// 	} else if (cur_pgdir) {
	// 		printk("cur_pgdir: %x (invalid)\n", cur_pgdir);
	// 	} else {
	// 		printk("cur_pgdir: NULL\n", cur_pgdir);
	// 	}
	// #endif

	while (1) {
	}
}

void _log(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	acquire(&pr_lock);
	// 输出日志头
	printfNoLock("%s %s:%d \"%s\"" SGR_RESET ": ", FARM_INFO "[INFO]" SGR_RESET SGR_BLUE, file,
		     line, func);
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
	printfNoLock("%s %s:%d \"%s\"" SGR_RESET ": ", FARM_WARN "[INFO]" SGR_RESET SGR_YELLOW,
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
	printfNoLock("%s %s:%d \"%s\"" SGR_RESET ": ", FARM_ERROR "[INFO]" SGR_RESET SGR_RED, file,
		     line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	release(&pr_lock);

	va_end(ap);
}