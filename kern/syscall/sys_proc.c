#include <dev/timer.h>
#include <lib/log.h>
#include <lib/transfer.h>
#include <proc/cpu.h>
#include <proc/nanosleep.h>
#include <proc/sched.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <sys/syscall.h>

void sys_exit(err_t code) {
	thread_t *td = cpu_this()->cpu_running;
	log(LEVEL_GLOBAL, "thread %s exit with code %d\n", td->td_name, code);
	td->td_exitcode = code;
	// mtx_lock(&td->td_lock);
	td_destroy();
}

extern fileid_t file_load(const char *path, void **bin, size_t *size);
extern void file_unload(fileid_t file);

static u64 argc_count(pte_t *pt, char **argv) {
	u64 argc = 0;
	void *ptr;
	do {
		copy_in(pt, (u64)(&argv[argc++]), &ptr, sizeof(ptr));
	} while (ptr != NULL);
	return argc - 1;
}

err_t sys_exec(u64 path, char **argv, u64 envp) {
	// 拷贝可执行文件路径
	thread_t *td = cpu_this()->cpu_running;
	char buf[MAX_PROC_NAME_LEN];
	copy_in_str(td->td_pt, path, buf, MAX_PROC_NAME_LEN);

	// 从旧的用户栈拷贝参数到新的用户栈
	td_initustack(td, TD_TEMPUSTACK);
	td_setustack(td, argc_count(td->td_pt, argv), argv);
	// 将旧的用户栈回收，新的用户栈生效
	td->td_trapframe->sp += TD_TEMPUSTACK_OFFSET;
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 stackva = TD_USTACK + i * PAGE_SIZE;
		u64 tmpva = TD_TEMPUSTACK + i * PAGE_SIZE;
		// 解除旧的用户栈映射
		panic_on(ptUnmap(td->td_pt, stackva));
		// 映射新的用户栈（先映射防止引用计数为零被回收）
		u64 pa = pteToPa(ptLookup(td->td_pt, tmpva));
		panic_on(ptMap(td->td_pt, stackva, pa, PTE_R | PTE_W | PTE_U));
		// 解除临时用户栈映射
		panic_on(ptUnmap(td->td_pt, tmpva));
	}

	// 回收先前的代码段
	for (u64 va = 0; va < td->td_brk; va += PAGE_SIZE) {
		if (ptLookup(td->td_pt, va) & PTE_V) {
			panic_on(ptUnmap(td->td_pt, va));
		}
	}
	td->td_brk = 0;

	// 加载可执行文件到内核
	void *bin;
	size_t size;
	fileid_t file = file_load(buf, &bin, &size);
	// 加载代码段
	td_initucode(td, bin, size);
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
	return td_fork(cpu_this()->cpu_running, stack);
}

u64 sys_wait4(u64 pid, u64 status, u64 options) {
	return wait(cpu_this()->cpu_running, pid, status, options);
}

/**
 * @brief 执行线程睡眠
 * @param pTimeSpec 包含秒和微秒两个字段，指明进程要睡眠的时间数
 */
u64 sys_nanosleep(u64 pTimeSpec) {
	struct timespec timeSpec;
	copyIn(pTimeSpec, &timeSpec, sizeof(timeSpec));
	u64 usec = timeSpec.second * 1000000 + timeSpec.usec;
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
	return cpu_this()->cpu_running->td_pid; // todo
}

u64 sys_getppid() {
	return cpu_this()->cpu_running->td_parent->td_pid; // todo
}

clock_t sys_times(u64 utms) {
	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, utms, &td->td_times, sizeof(td->td_times));
	return ticks;
}
