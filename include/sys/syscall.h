#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <types.h>

#define SYSCALL_ERROR -1

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
void sys_sched_yield();
u64 sys_getpid();
u64 sys_getppid();
clock_t sys_times(u64 utms);

// 系统信息（sys_info）
void sys_uname(u64 upuname);
void sys_gettimeofday(u64 uptv, u64 uptz);

// 文件系统（sys_fs）
int sys_write(int fd, u64 buf, size_t count);
int sys_read(int fd, u64 buf, size_t count);
int sys_openat(int fd, u64 filename, int flags, mode_t mode);
int sys_close(int fd);
int sys_dup(int fd);
int sys_dup3(int fd_old, int fd_new);
int sys_getcwd(u64 buf, int size);
int sys_pipe2(u64 pfd);
int sys_chdir(u64 path);
int sys_mkdirat(int dirFd, u64 path, int mode);
int sys_mount(u64 special, u64 dir, u64 fstype, u64 flags, u64 data);
int sys_umount(u64 special, u64 flags);
int sys_linkat(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags);
int sys_unlinkat(int dirFd, u64 pPath);
void *sys_mmap(u64 start, size_t len, int prot, int flags, int fd, off_t off);
int sys_fstat(int fd, u64 pkstat);
int sys_getdents64(int fd, u64 buf, int len);

#endif // !_SYSCALL_H
