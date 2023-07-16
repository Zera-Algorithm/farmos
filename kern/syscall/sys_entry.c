#include <dev/sbi.h>
#include <lib/log.h>
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
    [SYS_exit] = {sys_exit, "exit"},
    [SYS_execve] = {sys_exec, "execve"},
    [SYS_clone] = {sys_clone, "clone"},
    [SYS_wait4] = {sys_wait4, "wait4"},
    [SYS_nanosleep] = {sys_nanosleep, "nanosleep"},
    [SYS_mmap] = {sys_mmap, "mmap"},
    [SYS_fstat] = {sys_fstat, "fstat"},
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
    [SYS_sched_yield] = {sys_sched_yield, "sched_yield"},
    [SYS_getpid] = {sys_getpid, "getpid"},
    [SYS_getppid] = {sys_getppid, "getppid"},
    [SYS_times] = {sys_times, "times"},
    [SYS_uname] = {sys_uname, "uname"},
    [SYS_gettimeofday] = {sys_gettimeofday, "gettimeofday"},
    [SYS_munmap] = {sys_unmap, "munmap"},
    [SYS_brk] = {sys_brk, "brk"},
};

/**
 * @brief 系统调用入口。会按照tf中传的参数信息（a0~a7）调用相应的系统调用函数，并将返回值保存在a0中
 *
 */

void syscall_entry(Trapframe *tf) {
	u64 sysno = tf->a7;
	syscall_function_t *sys_func = &sys_table[sysno];

	if (sys_func != NULL && sys_func->name != NULL) {
		log(LEVEL_GLOBAL, "Hart %d Thread %s called '%s'\n", cpu_this_id(),
		    cpu_this()->cpu_running->td_name, sys_func->name);
	}

	// S态时间审计
	// u64 startTime = getTime();
	if (sysno == 210) {
		SBI_SYSTEM_RESET(0, 0); // todotodo: sys_shutdown
	}
	// 系统调用最多6个参数
	u64 (*func)(u64, u64, u64, u64, u64, u64);

	// 根据反汇编结果，一个ecall占用4字节的空间
	tf->epc += 4;

	// 获取系统调用函数
	func = (u64(*)(u64, u64, u64, u64, u64, u64))sys_table[sysno].func;
	if (func == NULL) {
		tf->a0 = SYSCALL_ERROR;
		warn("unimplemented or unknown syscall: %d\n", sysno);
		return;
		// sys_exit(SYSCALL_ERROR);
	}

	// 将系统调用返回值放入a0寄存器
	tf->a0 = func(tf->a0, tf->a1, tf->a2, tf->a3, tf->a4, tf->a5);

	// // S态时间审计
	// u64 endTime = getTime();
	// myProc()->procTime.totalStime += (endTime - startTime);
}
