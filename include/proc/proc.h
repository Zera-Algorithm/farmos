#ifndef _PROC_H_
#define _PROC_H_

#include <fs/thread_fs.h>
#include <lib/queue.h>
#include <lock/mutex.h>
#include <param.h>
#include <proc/times.h>
#include <trap/trapframe.h>

typedef struct thread thread_t;

typedef enum state { UNUSED = 0, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE } state_t;

typedef struct proc {
	mutex_t p_lock;		 // 进程锁（已被全局初始化）
	LIST_ENTRY(proc) p_list; // 空闲列表链接（空闲列表锁保护）
	TAILQ_HEAD(thread_tailq_head, thread) p_threads; // 拥有的线程队列（进程锁保护）

	state_t p_status;	  // 进程状态（进程锁保护）
	pid_t p_pid;		  // 进程 id（不保护，进程初始化后只读）
	ptr_t p_brk;		  // 进程堆顶
	pte_t *p_pt;		  // 线程用户态页表（不保护，进程初始化后只读）
	trapframe_t *p_trapframe; // 用户态上下文头指针（不保护，进程初始化后只读）

#define p_startzero p_times
	err_t p_exitcode; // 进程退出码（进程锁保护）
	times_t p_times;  // 线程运行时间（进程锁保护）
#define p_endzero p_fs_struct

	thread_fs_t p_fs_struct; // 文件系统相关字段（不保护）

	struct proc *p_parent;	      // 父线程（不保护，只会由父进程修改）
	LIST_HEAD(, proc) p_children; // 子进程列表（由进程锁保护）
	LIST_ENTRY(proc) p_sibling;   // 子进程列表链接（由父进程锁保护）
} proc_t;

// 进程队列
typedef struct proclist {
	mutex_t pl_lock;	   // 进程队列锁
	LIST_HEAD(, proc) pl_list; // 进程队列
} proclist_t;

extern proc_t procs[NPROC];
extern proclist_t proc_freelist;

void proc_init();
proc_t *proc_alloc();
void proc_addtd(proc_t *p, thread_t *td);

typedef struct stack_arg stack_arg_t;

void proc_initupt(proc_t *p);
int proc_initucode_by_binary(proc_t *p, thread_t *inittd, const void *bin, size_t size, stack_arg_t *parg);

typedef struct stack_arg stack_arg_t;
typedef void (*argv_callback_t)(char *kstr_arr[]);

void proc_initustack(proc_t *p, thread_t *inittd);
void proc_recycleupt(proc_t *p);
stack_arg_t proc_setustack(thread_t *td, pte_t *argpt, u64 argc, char **argv, u64 envp,
		    argv_callback_t callback);

void proc_create(const char *name, const void *bin, size_t size);
u64 td_fork(thread_t *td, u64 childsp, u64 ptid, u64 tls, u64 ctid);
u64 proc_fork(thread_t *td, u64 childsp, u64 flags);

void proc_destroy(proc_t *p, err_t exitcode);
void proc_free(proc_t *p);

// 相关宏
#define PROC_CREATE(program, name)                                                                 \
	({                                                                                         \
		extern char binary_##program[];                                                    \
		extern int binary_##program##_size;                                                \
		proc_create(name, binary_##program, binary_##program##_size);                      \
	})

#define plist_critical_enter(plist) mtx_lock(&(plist)->pl_lock)
#define plist_critical_exit(plist) mtx_unlock(&(plist)->pl_lock)

#define proc_lock(p) mtx_lock(&(p)->p_lock)
#define proc_unlock(p) mtx_unlock(&(p)->p_lock)
#define proc_hold(p) mtx_hold(&(p)->p_lock)

#define PID_GENERATE(cnt, index) ((index) | ((cnt % 0x1000) << 16))
#define PID_TO_INDEX(tid) (tid & 0xffff)
#define PID_INIT (PID_GENERATE(1, 0))

#endif /* !_PROC_H_ */
