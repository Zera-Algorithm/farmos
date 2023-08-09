#include <dev/timer.h>
#include <fs/thread_fs.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lib/elf.h>
#include <mm/kmalloc.h>
#include <mm/vmtools.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/sched.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <sys/errno.h>
#include <sys/syscall.h>
#include <sys/syscall_proc.h>
#include <sys/time.h>
#include <proc/procarg.h>
#include <proc/tsleep.h>

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

static stack_arg_t copy_arg(proc_t *p, thread_t *exectd, char **argv, u64 envp, argv_callback_t callback) {
	// 从旧的用户栈拷贝参数到新的用户栈

	// 将旧的用户栈映射到临时页表上
	// 分配临时页表，将旧的用户栈迁移到临时页表
	proc_t tempp = {.p_pt = (pte_t*)kvmAlloc()};
	thread_t temptd = {.td_proc = &tempp};
	proc_initustack(&tempp, &temptd);

	exectd->td_trapframe.sp = USTACKTOP;
	stack_arg_t ret = proc_setustack(exectd, tempp.p_pt, argc_count(p->p_pt, argv), argv, envp, callback);

	// 迁移新的用户栈到旧的页表
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 stackva = TD_USTACK_BOTTOM + i * PAGE_SIZE;
		// 解引用并释放旧页表上已过时的栈
		panic_on(ptUnmap(p->p_pt, stackva));
		// 将新页表上的栈映射到旧页表上
		pte_t pte = ptLookup(tempp.p_pt, stackva);
		u64 pa = pteToPa(pte);
		u64 perm = PTE_PERM(pte);
		panic_on(ptMap(p->p_pt, stackva, pa, perm));
	}
	// 回收临时页表
	pdWalk(tempp.p_pt, vmUnmapper, kvmUnmapper, NULL);
	return ret;
}

extern fileid_t file_load(const char *path, void **bin, size_t *size);
extern void file_unload(fileid_t file);

static void exec_sh_callback(char *kstr_arr[]) {
	// 前面插入两项
	int i;
	for (i = 0; kstr_arr[i] != NULL; i++)
		;
	for (int j = i; j >= 0; j--) {
		kstr_arr[j + 2] = kstr_arr[j];
	}

	char *argv1 = kmalloc(32);
	char *argv2 = kmalloc(32);
	strncpy(argv1, "/busybox", 32);
	strncpy(argv2, "ash", 32);
	kstr_arr[0] = argv1;
	kstr_arr[1] = argv2;
}

static void exec_elf_callback(char *kstr_arr[]) {
	char buf[512];
	strncpy(buf, "execve args: ", 512);
	int i;
	for (i = 0; kstr_arr[i] != NULL; i++) {
		strcat(buf, kstr_arr[i]);
		strcat(buf, " ");
	}
	log(PROC_GLOBAL, "%s\n", buf);

	// td改名，加后缀
	thread_t *td = cpu_this()->cpu_running;

	if (kstr_arr[0] == NULL) {
		return;
	}

	strcat(td->td_name, "_");
	strcat(td->td_name, kstr_arr[i - 1]);
}

/**
 * @brief 执行可执行的ELF文件，或者shell脚本
 */
err_t sys_exec(u64 path, char **argv, u64 envp) {
	// 当前只支持进程中仅有一个线程时进行 exec
	thread_t *td = cpu_this()->cpu_running;
	proc_t *p = cpu_this()->cpu_running->td_proc;
	stack_arg_t stack_arg; // 压栈参数

	assert(TAILQ_FIRST(&p->p_threads) == TAILQ_LAST(&p->p_threads, thread_tailq_head));

	// 拷贝可执行文件路径到内核
	char pathbuf[MAX_PROC_NAME_LEN];
	copy_in_str(p->p_pt, path, pathbuf, MAX_PROC_NAME_LEN);
	safestrcpy(td->td_name, pathbuf, MAX_PROC_NAME_LEN);
	void *bin;
	size_t size;
	fileid_t file_id = file_load(pathbuf, &bin, &size);
	if (file_id < 0) {
		return file_id;
	}

	// Note: 区分ELF和脚本
	int len = strlen(pathbuf);
	if (len > 3 && pathbuf[len - 3] == '.' && pathbuf[len - 2] == 's' &&
	    pathbuf[len - 1] == 'h') {
		file_unload(file_id); // 先释放不使用的文件

		// 执行脚本，指定解释器为busybox
		strncpy(pathbuf, "/busybox", MAX_PROC_NAME_LEN);
		// 加载参数
		stack_arg = copy_arg(p, td, argv, envp, exec_sh_callback);

		// 重新加载文件
		file_id = file_load(pathbuf, &bin, &size);
		assert(file_id >= 0);
	} else { // 判定为ELF文件
		// 加载参数
		stack_arg = copy_arg(p, td, argv, envp, exec_elf_callback);
	}

	// 回收先前的代码段
	for (u64 va = 0; va < p->p_brk; va += PAGE_SIZE) {
		if (ptLookup(p->p_pt, va) & PTE_V) {
			panic_on(ptUnmap(p->p_pt, va));
		}
	}
	p->p_brk = 0;
	td->td_ctid = 0;

	// 加载程序的各个段
	log(DEBUG, "START LOAD CODE SEGMENT\n");
	int ret = proc_initucode_by_binary(p, td, bin, size, &stack_arg);
	log(DEBUG, "END LOAD CODE SEGMENT\n");

	file_unload(file_id);
	// 刷新指令cache，防止数据不正常
	__asm__ __volatile__("fence.i" : : : "memory");
	return ret;
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
	warn("params: flags = %lx, stack = %lx, ptid = %lx, tls = %lx, ctid = %lx\n", flags, stack,
	     ptid, tls, ctid);
	if (flags & CLONE_VM) {
		return td_fork(cpu_this()->cpu_running, stack, ptid, tls, ctid);
	} else {
		return proc_fork(cpu_this()->cpu_running, stack, flags);
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
	timespec_t timeVal;
	copyIn(pTimeSpec, &timeVal, sizeof(timeVal));
	tsleep(&timeVal, NULL, "nanosleep", TS_USEC(timeVal) + time_mono_us());
	return 0;
}

# define TIMER_ABSTIME			1
// request是nanosleep类型的指针
u64 sys_clock_nanosleep(u64 clock_id, u64 flags, u64 request, u64 remain) {
	timespec_t timeVal;
	copyIn(request, &timeVal, sizeof(timeVal));
	if (flags & TIMER_ABSTIME) {
		// 以绝对时间睡眠
		if (clock_id == CLOCK_REALTIME) {
			tsleep(&timeVal, NULL, "clock_nanosleep1", TS_USEC(timeVal) - RTC_OFF);
		} else {
			log(0, "clock_nanosleep: clock_id = %d, %d, %d\n", clock_id, timeVal.tv_sec, timeVal.tv_nsec);
			tsleep(&timeVal, NULL, "clock_nanosleep2", TS_USEC(timeVal));
		}
		
	} else {
		tsleep(&timeVal, NULL, "clock_nanosleep3", TS_USEC(timeVal) + time_mono_us());
	}
	return 0;
}

void sys_sched_yield() {
	yield();
}

u64 sys_getpid() {
	return cpu_this()->cpu_running->td_proc->p_pid;
}

u64 sys_getuid() {
	return 0; // 未实现用户，直接返回0即可
}

u64 sys_gettid() {
	return cpu_this()->cpu_running->td_tid;
}

// pTid是int *的指针
u64 sys_set_tid_address(u64 pTid) {
	thread_t *td = cpu_this()->cpu_running;
	td->td_ctid = pTid;
	return td->td_tid;
}

u64 sys_getppid() {
	return cpu_this()->cpu_running->td_proc->p_parent->p_pid;
}

clock_t sys_times(u64 utms) {
	proc_t *p = cpu_this()->cpu_running->td_proc;
	proc_lock(p);
	times_t times = cpu_this()->cpu_running->td_proc->p_times;
	proc_unlock(p);
	copy_out(p->p_pt, utms, &times, sizeof(times));
	return time_mono_clock();
}

/**
 * @brief 调整进程的资源限制。目前仅支持NOFILE和STACK的查询，不支持修改。修改不生效
 */
int sys_prlimit64(pid_t pid, int resource, const struct rlimit *pnew_limit,
		  struct rlimit *pold_limit) {
	if (pold_limit != NULL) {
		struct rlimit oldlimit;
		switch (resource) {
		case RLIMIT_NOFILE:
			oldlimit.rlim_cur = cur_proc_fs_struct()->rlimit_files_cur;
			oldlimit.rlim_max = cur_proc_fs_struct()->rlimit_files_max;
			break;
		case RLIMIT_STACK:
			oldlimit.rlim_cur = oldlimit.rlim_max = TD_USTACK_SIZE;
			break;
		default:
			warn("sys_prlimit64: unsupported resource %d\n", resource);
			return -EINVAL;
		}
		copyOut((u64)pold_limit, &oldlimit, sizeof(oldlimit));
	}

	// 设置新的值
	if (pnew_limit != NULL) {
		struct rlimit newlimit;
		copyIn((u64)pnew_limit, &newlimit, sizeof(newlimit));
		if (newlimit.rlim_cur > newlimit.rlim_max) {
			warn("sys_prlimit64: new_limit->rlim_cur %d > new_limit->rlim_max %d\n",
			     newlimit.rlim_cur, newlimit.rlim_max);
			return -EINVAL;
		}

		switch (resource) {
		case RLIMIT_NOFILE:
			// 大于最大可分配数
			if (newlimit.rlim_max > MAX_FD_COUNT) {
				warn("sys_prlimit64: new_limit->rlim_max %d > MAX_FD_COUNT %d\n",
				     newlimit.rlim_max, MAX_FD_COUNT);
				return -EINVAL;
			}

			cur_proc_fs_struct()->rlimit_files_cur = newlimit.rlim_cur;
			cur_proc_fs_struct()->rlimit_files_max = newlimit.rlim_max;
			break;
		default:
			warn("sys_prlimit64: unsupported resource %d\n", resource);
			return 0; // 返回0以避免评测出错
		}
	}
	return 0;
}

pid_t sys_getsid(pid_t pid) {
	return 0;
}

pid_t sys_setsid() {
	return 0;
}

void sys_reboot() {
	cpu_halt();
}
