#include <lib/log.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/syscall.h>
#include <sys/syscall_ids.h>
#include <types.h>

static void *syscallTable[] = {
    [1023] = NULL,
    [SYS_openat] = sys_openat,
    [SYS_read] = sys_read,
    [SYS_write] = sys_write,
    [SYS_exit] = sys_exit,
    [SYS_execve] = sys_exec,
    [SYS_clone] = sys_clone,
    [SYS_wait4] = sys_wait4,
    [SYS_nanosleep] = sys_nanosleep,
    [SYS_mmap] = sys_mmap,
    [SYS_fstat] = sys_fstat,
    [SYS_close] = sys_close,
    [SYS_dup] = sys_dup,
    [SYS_dup3] = sys_dup3,
    [SYS_getcwd] = sys_getcwd,
    [SYS_pipe2] = sys_pipe2,
    [SYS_chdir] = sys_chdir,
    [SYS_mkdirat] = sys_mkdirat,
    [SYS_mount] = sys_mount,
    [SYS_mount] = sys_umount,
    [SYS_linkat] = sys_linkat,
    [SYS_unlinkat] = sys_unlinkat,
};

/**
 * @brief 系统调用入口。会按照tf中传的参数信息（a0~a7）调用相应的系统调用函数，并将返回值保存在a0中
 *
 */

void syscall_entry(Trapframe *tf) {
	log(LEVEL_GLOBAL, "cpu %d, syscall %d, proc %lx\n", cpu_this_id(), tf->a7,
	    cpu_this()->cpu_running->td_tid);

	// S态时间审计
	// u64 startTime = getTime();

	u64 sysno = tf->a7;
	// 系统调用最多6个参数
	u64 (*func)(u64, u64, u64, u64, u64, u64);

	// 根据反汇编结果，一个ecall占用4字节的空间
	tf->epc += 4;

	// 获取系统调用函数
	func = (u64(*)(u64, u64, u64, u64, u64, u64))syscallTable[sysno];
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
