#include <dev/sbi.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/syscall.h>
#include <sys/syscall_ids.h>
#include <types.h>

typedef struct syscall_function {
	void *func;
	const char *name;
} syscall_function_t;

static syscall_function_t sys_table[] = {
    [1023] = {NULL, NULL},
    [SYS_openat] = {sys_openat, "openat"},
    [SYS_read] = {sys_read, "read"},
    [SYS_write] = {sys_write, "write"},
    [SYS_pread64] = {sys_pread64, "pread64"},
    [SYS_pwrite64] = {sys_pwrite64, "pwrite64"},
    [SYS_readv] = {sys_readv, "readv"},
    [SYS_writev] = {sys_writev, "writev"},
    [SYS_exit] = {sys_exit, "exit"},
    [SYS_execve] = {sys_exec, "execve"},
    [SYS_clone] = {sys_clone, "clone"},
    [SYS_wait4] = {sys_wait4, "wait4"},
    [SYS_nanosleep] = {sys_nanosleep, "nanosleep"},
    [SYS_mmap] = {sys_mmap, "mmap"},
    [SYS_mprotect] = {sys_mprotect, "mprotect"},
    [SYS_msync] = {sys_msync, "msync"},
    [SYS_madvise] = {sys_madvise, "sys_madvise"},
    [SYS_fstat] = {sys_fstat, "fstat"},
    [SYS_fstatat] = {sys_fstatat, "fstatat"},
    [SYS_faccessat] = {sys_faccessat, "faccessat"},
	[SYS_ftruncate] = {sys_ftruncate, "ftruncate"},
    [SYS_close] = {sys_close, "close"},
    [SYS_dup] = {sys_dup, "dup"},
    [SYS_dup3] = {sys_dup3, "dup3"},
    [SYS_getcwd] = {sys_getcwd, "getcwd"},
    [SYS_pipe2] = {sys_pipe2, "pipe2"},
    [SYS_chdir] = {sys_chdir, "chdir"},
    [SYS_mkdirat] = {sys_mkdirat, "mkdirat"},
    [SYS_mount] = {sys_mount, "mount"},
    [SYS_umount2] = {sys_umount, "umount2"},
    [SYS_linkat] = {sys_linkat, "linkat"},
    [SYS_unlinkat] = {sys_unlinkat, "unlinkat"},
    [SYS_getdents64] = {sys_getdents64, "getdents64"},
    [SYS_utimensat] = {sys_utimensat, "utimensat"},
    [SYS_renameat2] = {sys_renameat2, "renameat2"},
    [SYS_ioctl] = {sys_ioctl, "ioctl"},
    [SYS_ppoll] = {sys_ppoll, "ppoll"},
    [SYS_fcntl] = {sys_fcntl, "fcntl"},
    [SYS_lseek] = {sys_lseek, "lseek"},
    [SYS_sched_yield] = {sys_sched_yield, "sched_yield"},
    [SYS_gettid] = {sys_gettid, "gettid"},
    [SYS_getpid] = {sys_getpid, "getpid"},
    [SYS_getppid] = {sys_getppid, "getppid"},
    [SYS_getuid] = {sys_getuid, "getuid"},
    [SYS_set_tid_address] = {sys_set_tid_address, "set_tid_address"},
    [SYS_times] = {sys_times, "times"},
    [SYS_uname] = {sys_uname, "uname"},
    [SYS_clock_gettime] = {sys_clock_gettime, "clock_gettime"},
    [SYS_gettimeofday] = {sys_gettimeofday, "gettimeofday"},
    [SYS_munmap] = {sys_unmap, "munmap"},
    [SYS_brk] = {sys_brk, "brk"},
    [SYS_socket] = {sys_socket, "socket"},
    [SYS_bind] = {sys_bind, "bind"},
    [SYS_listen] = {sys_listen, "listen"},
    [SYS_connect] = {sys_connect, "connect"},
    [SYS_accept] = {sys_accept, "accept"},
    [SYS_recvfrom] = {sys_recvfrom, "recvfrom"},
    [SYS_sendto] = {sys_sendto, "sendto"},
    [SYS_getsockname] = {sys_getsocketname, "getsockname"},
	[SYS_getpeername] = {sys_getpeername, "getpeername"},
    [SYS_getsockopt] = {sys_getsockopt, "getsockopt"},
    [SYS_setsockopt] = {sys_setsockopt, "setsockopt"},
	[SYS_membarrier] = {sys_membarrier, "membarrier"},
    [SYS_rt_sigaction] = {sys_sigaction, "sigaction"},
    [SYS_rt_sigreturn] = {sys_sigreturn, "sigreturn"},
    [SYS_rt_sigprocmask] = {sys_sigprocmask, "sigprocmask"},
    [SYS_tkill] = {sys_tkill, "tkill"},
	[SYS_setitimer] = {sys_setitimer, "setitimer"},
	[SYS_getitimer] = {sys_getitimer, "getitimer"},
    [SYS_prlimit64] = {sys_prlimit64, "prlimit64"},
    [SYS_kill] = {sys_kill, "kill"},
    [SYS_futex] = {sys_futex, "futex"},
    [SYS_statfs] = {sys_statfs, "statfs"},
    [SYS_rt_sigtimedwait] = {sys_sigtimedwait, "sigtimedwait"},
	[SYS_getsid] = {sys_getsid, "getsid"},
	[SYS_setsid] = {sys_setsid, "setsid"},
	[SYS_pselect6] = {sys_pselect6, "pselect6"},
};

/**
 * @brief 系统调用入口。会按照tf中传的参数信息（a0~a7）调用相应的系统调用函数，并将返回值保存在a0中
 *
 */
extern char *sys_names[];

void syscall_entry(trapframe_t *tf) {
	u64 sysno = tf->a7;
	syscall_function_t *sys_func = &sys_table[sysno];
    thread_t *td = cpu_this()->cpu_running;

	if (sys_func != NULL && sys_func->name != NULL) {
		if (sysno != SYS_brk)
			log(LEVEL_GLOBAL, "Hart %d Thread %s called '%s', epc = %lx\n", cpu_this_id(),
		    	td->td_name, sys_func->name, tf->epc);
        if (sysno != SYS_brk)
        	log(LEVEL_GLOBAL, "Thread %08x(p %08x) called '%s' start\n", td->td_tid, td->td_proc->p_pid, sys_func->name);
	}


	// S态时间审计
	// u64 startTime = getTime();
	// 系统调用最多6个参数
	u64 (*func)(u64, u64, u64, u64, u64, u64);

	// 根据反汇编结果，一个ecall占用4字节的空间
	tf->epc += 4;

	// 获取系统调用函数
	func = (u64(*)(u64, u64, u64, u64, u64, u64))sys_table[sysno].func;
	if (func == NULL) {
		// TODO: 未实现的syscall应当默认返回-1
		tf->a0 = -1;
		warn("unimplemented or unknown syscall: %s(%d)\n", sys_names[sysno], sysno);
		return;
		// sys_exit(SYSCALL_ERROR);
	}

	// 将系统调用返回值放入a0寄存器
	tf->a0 = func(tf->a0, tf->a1, tf->a2, tf->a3, tf->a4, tf->a5);
	if ((i64)tf->a0 < 0) {
		warn("ERROR: syscall %s(%d) returned %d\n", sys_names[sysno], sysno, tf->a0);
	}
    if (sysno != SYS_brk)
    	log(LEVEL_GLOBAL, "Thread %s %08x called '%s' return 0x%lx\n", cpu_this()->cpu_running->td_name, cpu_this()->cpu_running->td_tid, sys_func->name, tf->a0);

	// // S态时间审计
	// u64 endTime = getTime();
	// myProc()->procTime.totalStime += (endTime - startTime);
}
