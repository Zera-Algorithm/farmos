#include <dev/interface.h>
#include <lib/printf.h>
#include <lib/terminal.h>
#include <lib/vprint.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <riscv.h>
#include <mm/memlayout.h>
#include <mm/kmalloc.h>

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
		cons_putc(buf[i]);
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

// sprintf无需加锁
void sprintf(char *buf, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char *mybuf = buf;

	vprintfmt(outputToStr, &mybuf, fmt, ap);

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

static void print_stack(u64 kstack) {
	printf("panic in Thread %s. kernel sp = %lx\n", cpu_this()->cpu_running->td_name, kstack);

	extern thread_t *threads;
	extern void *kstacks;
	u64 stackTop = (u64)kstacks + TD_KSTACK_SIZE * (cpu_this()->cpu_running - threads + 1);
	u64 epc;
	asm volatile("auipc %0, 0" : "=r"(epc));

	char *buf = kmalloc(32 * PAGE_SIZE);
	buf[0] = 0;
	char *pbuf = buf;
	if (stackTop - TD_KSTACK_SIZE < kstack && kstack <= stackTop) {
		printf("Kernel Stack Used: 0x%lx\n", stackTop - kstack);
		printf(pbuf, "Memory(Start From 0x%016lx)\n", kstack);
		sprintf(pbuf, "0x%016lx \'[", kstack);
		pbuf += 21;
		for (u64 sp = kstack; sp < stackTop; sp++) {
			char ch = *(char *)sp; // 内核地址，可以直接访问
			sprintf(pbuf, "0x%02x, ", ch);
			pbuf += 6;
		}
		sprintf(pbuf, "]\' kern/kernel.asm 0x%016lx", epc);
		printf("%s\n", buf);
	} else {
		printf("[ERROR] sp is out of ustack range[0x%016lx, 0x%016lx]. Maybe program has crashed!\n", stackTop - TD_KSTACK_SIZE, stackTop);
	}
	kfree(buf);
}


void _error(const char *file, int line, const char *func, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	u64 sp;
	asm volatile("mv %0, sp" : "=r"(sp));
	print_stack(sp);

	mtx_lock(&pr_lock);
	// 输出日志头
	printfNoLock("%s %2d %12s:%-4d %12s()  !TEST FINISH! " SGR_RESET ": ",
		     FARM_ERROR "[ERROR]" SGR_RESET SGR_RED, cpu_this_id(), file, line, func);
	// 输出实际内容
	vprintfmt(output, NULL, fmt, ap);
	printfNoLock("\n\n");
	mtx_unlock(&pr_lock);

	va_end(ap);

	cpu_halt();

	while (1)
		;
}

/**
 * @brief 在发生异常时，打印寄存器的信息
 */
void printReg(struct trapframe *tf) {
	mtx_lock(&pr_lock);

	printfNoLock("ra  = 0x%016lx\t", tf->ra);
	printfNoLock("sp  = 0x%016lx\t", tf->sp);
	printfNoLock("gp  = 0x%016lx\n", tf->gp);
	printfNoLock("tp  = 0x%016lx\t", tf->tp);
	printfNoLock("t0  = 0x%016lx\t", tf->t0);
	printfNoLock("t1  = 0x%016lx\n", tf->t1);
	printfNoLock("t2  = 0x%016lx\t", tf->t2);
	printfNoLock("s0  = 0x%016lx\t", tf->s0);
	printfNoLock("s1  = 0x%016lx\n", tf->s1);
	printfNoLock("a0  = 0x%016lx\t", tf->a0);
	printfNoLock("a1  = 0x%016lx\t", tf->a1);
	printfNoLock("a2  = 0x%016lx\n", tf->a2);
	printfNoLock("a3  = 0x%016lx\t", tf->a3);
	printfNoLock("a4  = 0x%016lx\t", tf->a4);
	printfNoLock("a5  = 0x%016lx\n", tf->a5);
	printfNoLock("a6  = 0x%016lx\t", tf->a6);
	printfNoLock("a7  = 0x%016lx\t", tf->a7);
	printfNoLock("s2  = 0x%016lx\n", tf->s2);
	printfNoLock("s3  = 0x%016lx\t", tf->s3);
	printfNoLock("s4  = 0x%016lx\t", tf->s4);
	printfNoLock("s5  = 0x%016lx\n", tf->s5);
	printfNoLock("s6  = 0x%016lx\t", tf->s6);
	printfNoLock("s7  = 0x%016lx\t", tf->s7);
	printfNoLock("s8  = 0x%016lx\n", tf->s8);
	printfNoLock("s9  = 0x%016lx\t", tf->s9);
	printfNoLock("s10 = 0x%016lx\t", tf->s10);
	printfNoLock("s11 = 0x%016lx\n", tf->s11);
	printfNoLock("t3  = 0x%016lx\t", tf->t3);
	printfNoLock("t4  = 0x%016lx\t", tf->t4);
	printfNoLock("t5  = 0x%016lx\n", tf->t5);
	printfNoLock("t6  = 0x%016lx\n", tf->t6);

	mtx_unlock(&pr_lock);
}
