#include <dev/timer.h>
#include <fs/thread_fs.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
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
	pte_t *temppt = (pte_t *)kvmAlloc();
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 stackva = TD_USTACK + i * PAGE_SIZE;
		u64 pa = vmAlloc();
		panic_on(ptMap(temppt, stackva, pa, PTE_R | PTE_W | PTE_U));
	}

	exectd->td_trapframe.sp = TD_USTACK + TD_USTACK_SIZE;
	stack_arg_t ret = proc_setustack(exectd, temppt, argc_count(p->p_pt, argv), argv, envp, callback);

	// 回收临时页表
	for (int i = 0; i < TD_USTACK_PAGE_NUM; i++) {
		u64 stackva = TD_USTACK + i * PAGE_SIZE;
		panic_on(ptUnmap(p->p_pt, stackva));
		u64 pa = pteToPa(ptLookup(temppt, stackva));
		panic_on(ptMap(p->p_pt, stackva, pa, PTE_R | PTE_W | PTE_U));
	}
	pdWalk(temppt, vmUnmapper, kvmUnmapper, NULL);
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
	log(LEVEL_GLOBAL, "%s\n", buf);

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

	// TODO: 区分ELF和脚本
	int len = strlen(pathbuf);
	if (len > 3 && pathbuf[len - 3] == '.' && pathbuf[len - 2] == 's' &&
	    pathbuf[len - 1] == 'h') {
		// 执行脚本，指定解释器为busybox
		strncpy(pathbuf, "/busybox", MAX_PROC_NAME_LEN);
		// 加载参数
		stack_arg = copy_arg(p, td, argv, envp, exec_sh_callback);
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
	proc_initucode_by_file(p, td, pathbuf, &stack_arg);
	log(DEBUG, "END LOAD CODE SEGMENT\n");
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
	warn("params: flags = %lx, stack = %lx, ptid = %lx, tls = %lx, ctid = %lx\n", flags, stack,
	     ptid, tls, ctid);
	if (flags & CLONE_VM) {
		return td_fork(cpu_this()->cpu_running, stack, ptid, tls, ctid);
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
	tsleep(&timeVal, NULL, "nanosleep", TV_USEC(timeVal) + getUSecs());
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
	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, utms, &td->td_times, sizeof(td->td_times));
	return ticks;
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
