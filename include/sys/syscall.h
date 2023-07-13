#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <types.h>

#define SYSCALL_ERROR -1

struct timespec {
	uint64 second;
	long usec;
};

// 系统调用入口
typedef struct trapframe trapframe_t;
void syscall_entry(trapframe_t *tf);

// 内存管理（sys_mem）
err_t sys_map(u64 start, u64 len, u64 perm);
err_t sys_unmap(u64 start, u64 len);
err_t sys_brk(u64 addr);

// 进程管理（sys_proc）
void sys_exit(err_t code) __attribute__((noreturn));
err_t sys_exec(u64 path, char **argv, u64 envp);
u64 sys_clone(u64 flags, u64 stack, u64 ptid, u64 tls, u64 ctid);
u64 sys_wait4(u64 pid, u64 status, u64 options);
u64 sys_nanosleep(u64 pTimeSpec);

// 文件系统（sys_fs）
int sys_write(int fd, u64 buf, size_t count);
int sys_read(int fd, u64 buf, size_t count);
int sys_openat(int fd, u64 filename, int flags, mode_t mode);
void *sys_mmap(u64 start, size_t len, int prot, int flags, int fd, off_t off);
int sys_fstat(int fd, u64 pkstat);

#endif // !_SYSCALL_H
