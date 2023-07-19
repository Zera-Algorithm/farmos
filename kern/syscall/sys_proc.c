#include <dev/timer.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmtools.h>
#include <proc/cpu.h>
#include <proc/nanosleep.h>
#include <proc/sched.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <sys/syscall.h>
#include <sys/syscall_proc.h>
#include <sys/time.h>

void sys_exit(err_t code) {
	thread_t *td = cpu_this()->cpu_running;
	log(LEVEL_GLOBAL, "thread %s exit with code %d\n", td->td_name, code);
	td_destroy(code);
	error("sys_exit should not return");
}

// extern fileid_t file_load(const char *path, void **bin, size_t *size);
// extern void file_unload(fileid_t file);

static u64 argc_count(pte_t *pt, char **argv) {
	u64 argc = 0;
	void *ptr;
	do {
		copy_in(pt, (u64)(&argv[argc++]), &ptr, sizeof(ptr));
	} while (ptr != NULL);
	return argc - 1;
}

static void copy_arg(proc_t *p, thread_t *exectd, char **argv, u64 envp) {
	// 从旧的用户栈拷贝参数到新的用户栈

	// 将旧的用户栈映射到临时页表上
	// 分配临时页表，将旧的用户栈迁移到临时页表
	pte_t *temppt = (pte_t *)kvmAlloc();
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 stackva = TD_USTACK + i * PAGE_SIZE;
		u64 pa = vmAlloc();
		panic_on(ptMap(temppt, stackva, pa, PTE_R | PTE_W | PTE_U));
	}

	exectd->td_trapframe.sp = TD_USTACK + TD_USTACK_SIZE;
	proc_setustack(exectd, temppt, argc_count(p->p_pt, argv), argv, envp);

	// 回收临时页表
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 stackva = TD_USTACK + i * PAGE_SIZE;
		panic_on(ptUnmap(p->p_pt, stackva));
		u64 pa = pteToPa(ptLookup(temppt, stackva));
		panic_on(ptMap(p->p_pt, stackva, pa, PTE_R | PTE_W | PTE_U));
	}
	pdWalk(temppt, vmUnmapper, kvmUnmapper, NULL);
}

extern fileid_t file_load(const char *path, void **bin, size_t *size);
extern void file_unload(fileid_t file);

err_t sys_exec(u64 path, char **argv, u64 envp) {
	// 当前只支持进程中仅有一个线程时进行 exec
	thread_t *td = cpu_this()->cpu_running;
	proc_t *p = cpu_this()->cpu_running->td_proc;

	assert(TAILQ_FIRST(&p->p_threads) == TAILQ_LAST(&p->p_threads, thread_tailq_head));

	// 拷贝可执行文件路径到内核
	char buf[MAX_PROC_NAME_LEN];
	copy_in_str(p->p_pt, path, buf, MAX_PROC_NAME_LEN);
	safestrcpy(td->td_name, buf, MAX_PROC_NAME_LEN);

	// 加载参数
	copy_arg(p, td, argv, envp);

	// 回收先前的代码段
	for (u64 va = 0; va < p->p_brk; va += PAGE_SIZE) {
		if (ptLookup(p->p_pt, va) & PTE_V) {
			panic_on(ptUnmap(p->p_pt, va));
		}
	}
	p->p_brk = 0;

	// 加载可执行文件到内核
	void *bin;
	size_t size;
	log(DEBUG, "START LOAD CODE\n");
	fileid_t file = file_load(buf, &bin, &size);
	log(DEBUG, "END LOAD CODE\n");
	// 加载代码段
	log(DEBUG, "START LOAD CODE SEGMENT\n");
	proc_initucode(p, td, bin, size);
	log(DEBUG, "END LOAD CODE SEGMENT\n");
	file_unload(file);
	return 0;
}

/**
 * @brief 克隆一个子线程 todo
 * @param flags 克隆选项。SIGCHLD：克隆子进程；
 * @param stack 进程的栈顶，为0表示使用默认栈顶（用户空间顶部）
 * @param ptid 父线程id，ignored
 * @param tls TLS线程本地存储描述符，ignored
 * @param ctid 子线程id，ignored
 * @return 成功返回子进程的id，失败返回-1
 */
u64 sys_clone(u64 flags, u64 stack, u64 ptid, u64 tls, u64 ctid) {
	if (flags & CLONE_VM) {
		return td_fork(cpu_this()->cpu_running, stack);
	} else {
		return proc_fork(cpu_this()->cpu_running, stack);
	}
}

u64 sys_wait4(u64 pid, u64 status, u64 options) {
	// panic("sys_wait4 not implemented");
	return wait(cpu_this()->cpu_running, pid, status, options);
}

/**
 * @brief 执行线程睡眠
 * @param pTimeSpec 包含秒和微秒两个字段，指明进程要睡眠的时间数
 */
u64 sys_nanosleep(u64 pTimeSpec) {
	timeval_t timeVal;
	copyIn(pTimeSpec, &timeVal, sizeof(timeVal));
	u64 usec = timeVal.tv_sec * 1000000 + timeVal.tv_usec;
	u64 clocks = usec * CLOCK_PER_USEC;
	log(LEVEL_MODULE, "time to nanosleep: %d clocks\n", clocks);

	// 执行睡眠
	nanosleep_proc(clocks);

	return 0;
}

void sys_sched_yield() {
	yield();
}

u64 sys_getpid() {
	return cpu_this()->cpu_running->td_proc->p_pid;
}

u64 sys_getppid() {
	return cpu_this()->cpu_running->td_proc->p_parent->p_pid;
}

clock_t sys_times(u64 utms) {
	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, utms, &td->td_times, sizeof(td->td_times));
	return ticks;
}
