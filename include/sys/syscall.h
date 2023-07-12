#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <types.h>

#define SYSCALL_ERROR -1

// 系统调用入口
typedef struct trapframe trapframe_t;
void syscall_entry(trapframe_t *tf);

// 内存管理（sys_mem）
err_t sys_unmap(u64 start, u64 len);
err_t sys_brk(u64 addr);

// 进程管理（sys_proc）
void sys_exit(err_t code) __attribute__((noreturn));
err_t sys_exec(u64 path, char **argv, u64 envp);
u64 sys_clone(u64 flags, u64 stack, u64 ptid, u64 tls, u64 ctid);

#endif // !_SYSCALL_H