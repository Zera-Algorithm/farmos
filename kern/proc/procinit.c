#include <lib/elf.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <mm/vmtools.h>
#include <param.h>
#include <proc/proc.h>
#include <proc/sleep.h>
#include <proc/thread.h>

threadq_t thread_runq;
threadq_t thread_freeq;
threadq_t thread_sleepq;
thread_t threads[NPROC];

proclist_t proc_freelist;
proc_t procs[NPROC];

void *kstacks;

void thread_init() {
	extern mutex_t td_tid_lock;
	extern mutex_t mtx_nanosleep;
	mtx_init(&td_tid_lock, "td_tid_lock", false, MTX_SPIN);
	mtx_init(&wait_lock, "wait_lock", false, MTX_SPIN);
	mtx_init(&thread_runq.tq_lock, "thread_runq", false, MTX_SPIN);
	mtx_init(&thread_freeq.tq_lock, "thread_freeq", false, MTX_SPIN);
	mtx_init(&thread_sleepq.tq_lock, "thread_sleepq", false, MTX_SPIN);
	mtx_init(&mtx_nanosleep, "mtx_nanosleep", false, MTX_SPIN);
	TAILQ_INIT(&thread_runq.tq_head);
	TAILQ_INIT(&thread_freeq.tq_head);
	TAILQ_INIT(&thread_sleepq.tq_head);
	for (int i = NPROC - 1; i >= 0; i--) {
		thread_t *td = &threads[i];
		// 插入空闲线程队列
		TAILQ_INSERT_HEAD(&thread_freeq.tq_head, td, td_freeq);
		// 初始化线程锁
		mtx_init(&td->td_lock, "thread", false, MTX_SPIN | MTX_RECURSE);
		// 初始化线程内核栈
		td->td_kstack = (u64)kstacks + TD_KSTACK_SIZE * i;
		// 将内核线程栈映射到内核页表
		extern pte_t *kernPd;
		for (int j = 0; j < TD_KSTACK_PAGE_NUM; j++) {
			u64 pa = td->td_kstack + j * PAGE_SIZE;
			u64 va = TD_KSTACK(i) + j * PAGE_SIZE;
			panic_on(ptMap(kernPd, va, pa, PTE_R | PTE_W));
		}
	}
}

void proc_init() {
	extern mutex_t pid_lock;
	mtx_init(&pid_lock, "pid_lock", false, MTX_SPIN);
	mtx_init(&proc_freelist.pl_lock, "proc_freelist", false, MTX_SPIN);
	LIST_INIT(&proc_freelist.pl_list);
	for (int i = NPROC - 1; i >= 0; i--) {
		proc_t *p = &procs[i];
		// 插入空闲进程队列
		LIST_INSERT_HEAD(&proc_freelist.pl_list, p, p_list);
		// 初始化进程锁
		mtx_init(&p->p_lock, "proc", false, MTX_SPIN | MTX_RECURSE);
		// 初始化进程的线程队列
		TAILQ_INIT(&p->p_threads);
		// 初始化进程的子进程队列
		LIST_INIT(&p->p_children);
		// 初始化进程的父进程
		p->p_parent = NULL;
		// 初始化进程的页表
		p->p_pt = NULL;
		// 初始化进程的trapframe
		p->p_trapframe = NULL;
		// 初始化进程的用户栈
		p->p_brk = 0;
	}
}

/**
 * @brief 分配并初始化一个新的用户空间页表
 * @note 申请了用户页表、用户 Trapframe
 */
void proc_initupt(proc_t *p) {
	// 分配页表
	pte_t upt = kvmAlloc();
	p->p_pt = (pte_t *)upt;

	// TRAMPOLINE
	extern char trampoline[];
	// 由于TRAMPOLINE是用户与内核共享的空间，因此需要赋以 PTE_G 全局位
	panic_on(ptMap(p->p_pt, TRAMPOLINE, (u64)trampoline, PTE_R | PTE_X | PTE_G));

	// signal TRAMPOLINE
	extern char user_sig_return[];
	panic_on(ptMap(p->p_pt, SIGNAL_TRAMPOLINE, (u64)user_sig_return, PTE_R | PTE_X | PTE_U));

	// 该进程的trapframe
	p->p_trapframe = (trapframe_t *)vmAlloc();
	panic_on(ptMap(p->p_pt, TRAPFRAME, (u64)p->p_trapframe, PTE_R | PTE_W));
}

/**
 * @brief 分配并初始化一个新的用户空间栈
 * @note 申请了新的用户栈，并将其映射到用户页表，同时初始化用户栈指针
 */
void proc_initustack(proc_t *p, thread_t *inittd, u64 ustack) {
	// 分配用户栈空间
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 pa = vmAlloc();
		u64 va = ustack + i * PAGE_SIZE;
		panic_on(ptMap(p->p_pt, va, pa, PTE_R | PTE_W | PTE_U));
	}
	// 初始化用户栈空间指针
	inittd->td_trapframe.sp = ustack + TD_USTACK_SIZE;
	p->p_brk = 0;
}

void proc_recycleupt(proc_t *p) {
	// 解引用全部用户页表
	pdWalk(p->p_pt, vmUnmapper, kvmUnmapper, NULL);
	p->p_pt = 0;
	p->p_brk = 0;
}

// 初始化栈区域

/**
 * @brief 将内核的数据压到用户栈上
 * @param align 是否需要对其（16字节对齐）
 */
static inline void push_data(pte_t *argpt, u64 *sp, void *data, u64 len, int align) {
	// 将数据拷贝到用户栈
	*sp -= len;
	if (align) {
		*sp -= *sp % 16;
	}
	copy_out(argpt, *sp, data, len);
}

/**
 * @brief
 * 从in_pt对应的页表上取数据（通过arg_array指针），压到out_pt对应的栈上（栈指针为*p_sp）。新的栈上地址存储在arg_buf数组中
 * @param arg_array 指向参数字符串的用户地址空间指针，若为NULL表示没有参数
 * @return 返回arg_buf的长度
 */
static int copyin_arg_array(pte_t *in_pt, pte_t *out_pt, char **arg_array, u64 *p_sp,
			    char *arg_buf[]) {
	char buf[MAXARGLEN + 1];
	int arg_count = 0;

	if (arg_array != NULL) {
		while (1) {
			// 1. 拷贝指向参数字符串的用户地址空间指针
			char *arg;
			copy_in(in_pt, (u64)(&arg_array[arg_count]), &arg, sizeof(char *));
			if (arg == NULL) {
				break;
			}

			// 2. 从用户地址空间拷贝参数字符串
			copy_in_str(in_pt, (u64)arg, buf, MAXARGLEN);
			buf[MAXARGLEN] = '\0';

			size_t len = strlen(buf) + 1;
			buf[len - 1] = '\0';

			// 3. 向栈上压入argv字符串
			push_data(out_pt, p_sp, buf, len, true);

			// 4. 记录参数字符串的用户地址空间指针
			arg_buf[arg_count] = (char *)*p_sp;

			arg_count += 1;
			assert(arg_count < MAXARG);
		}
	}
	// 5. 参数数组以NULL表示结束
	arg_buf[arg_count] = NULL;

	return arg_count + 1;
}

/**
 * @brief 拷入额外的参数数组（arg_array来自内核）
 * @return 返回arg_buf的长度
 */
static int copyin_extra_arg_array(pte_t *out_pt, char **arg_array, u64 *p_sp, char *arg_buf[]) {
	int arg_count = 0;
	while (arg_buf[arg_count] != NULL) {
		arg_count += 1;
		assert(arg_count < MAXARG);
	}

	// 目前arg_buf[arg_count] == NULL
	for (int i = 0; arg_array[i] != NULL; i++) {
		// 1. 从内核拷贝参数字符串
		char *buf = arg_array[i];
		size_t len = strlen(buf) + 1;

		// 3. 向栈上压入argv字符串
		push_data(out_pt, p_sp, buf, len, true);

		// 4. 记录参数字符串的用户地址空间指针
		arg_buf[arg_count] = (char *)*p_sp;

		arg_count += 1;
		assert(arg_count < MAXARG);
	}

	// 5. 参数数组以NULL表示结束
	arg_buf[arg_count] = NULL;
	return arg_count + 1;
}

/**
 * @param auxiliary_vector 是一个二元组
 * @param p_len 表示arg_buf的长度
 */
static void append_auxiliary_vector(char *arg_buf[], u64 auxiliary_vector[], u64 *p_len) {
	arg_buf[*p_len] = (char *)auxiliary_vector[0];
	arg_buf[*p_len + 1] = (char *)auxiliary_vector[1];
	*p_len += 2;
}

static void build_auxiliary_vector(char *argvbuf[], u64 *p_len) {
	/**
	 * 辅助数组的字段由type和value两个组成，都是u64类型
	 * 辅助数组以NULL, NULL表示结束
	 */
	append_auxiliary_vector(argvbuf, (u64[]){AT_HWCAP, 0}, p_len);
	append_auxiliary_vector(argvbuf, (u64[]){AT_PAGESZ, PAGE_SIZE}, p_len);
	append_auxiliary_vector(argvbuf, (u64[]){AT_NULL, AT_NULL}, p_len);
}

/**
 * @brief 将给定的进程运行参数压入用户栈
 * @param td 线程指针
 * @param argpt 存储栈上内容的临时页表指针
 * @param argc 参数数量
 * @param argv 用户空间内的参数指针
 * @param envp 用户空间内的环境变量指针。为0表示没有环境变量
 */
void proc_setustack(thread_t *td, pte_t *argpt, u64 argc, char **argv, u64 envp) {
	pte_t *src_pt = td->td_proc->p_pt;
	// argv和envp需要并列在一个数组里面，以实现头部的对齐
	char *argvbuf[MAXARG];

	// 1. 拷入argv数组
	u64 len_argv = copyin_arg_array(src_pt, argpt, argv, &td->td_trapframe.sp, argvbuf);

	// // for debug
	// if (argv != NULL) {
	// 	u64 argv1;
	// 	char argv1_str[128];
	// 	copy_in(src_pt, (u64)(&argv[1]), &argv1, sizeof(char *));
	// 	copy_in_str(src_pt, (u64)argv1, argv1_str, 128);
	// 	warn("argv1 = %s\n", argv1_str);
	// 	strcat(td->td_name, "_");
	// 	strcat(td->td_name, argv1_str);
	// }
	// // debug end

	assert(len_argv == argc + 1);

	// 2. 拷入envp数组
	copyin_arg_array(src_pt, argpt, (char **)envp, &td->td_trapframe.sp, argvbuf + len_argv);

	// 测试拷入内核的参数(参数列表需要以NULL结尾)
	u64 len_envp = copyin_extra_arg_array(argpt, (char *[]){"LD_LIBRARY_PATH=/", NULL},
					      &td->td_trapframe.sp, argvbuf + len_argv);

	// 3. 拷入辅助数组
	u64 total_len = len_argv + len_envp;
	build_auxiliary_vector(argvbuf, &total_len);
	// reference: glibc

	// 4. 将argvbuf压入用户栈
	push_data(argpt, &td->td_trapframe.sp, argvbuf, total_len * sizeof(char *), true);

	argc = len_argv - 1;
	// 5. 将参数数量压入用户栈
	push_data(argpt, &td->td_trapframe.sp, &argc, sizeof(u64), false);

	// 6. 将参数放入寄存器
	// 通过 syscall 返回值实现 a0 = argc;
	// Note: 其实libc会自动设置a0=argc, a1=argv
	td->td_trapframe.a1 = td->td_trapframe.sp;
}
