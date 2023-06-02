#ifndef _PROC_H
#define _PROC_H

#include "param.h"
#include "riscv.h"
#include <lib/queue.h>
#include <lock/spinlock.h>
#include <mm/vmm.h>

// Per-CPU state.
struct cpu {
	struct Proc *proc; // The process running on this cpu, or null.
	int noff;	   // Depth of push_off() nesting.
	int intena;	   // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
typedef struct trapframe {
	u64 kernel_satp;  // 保存内核页表
	u64 trap_handler; // 内核态异常针对用户异常的处理函数（C函数）
	u64 epc;	  // 用户的epc
	u64 hartid;	  // 当前的hartid，取自tp寄存器
	u64 ra;
	u64 sp;
	u64 gp;
	u64 tp;
	u64 t0;
	u64 t1;
	u64 t2;
	u64 s0;
	u64 s1;
	u64 a0;
	u64 a1;
	u64 a2;
	u64 a3;
	u64 a4;
	u64 a5;
	u64 a6;
	u64 a7;
	u64 s2;
	u64 s3;
	u64 s4;
	u64 s5;
	u64 s6;
	u64 s7;
	u64 s8;
	u64 s9;
	u64 s10;
	u64 s11;
	u64 t3;
	u64 t4;
	u64 t5;
	u64 t6;
	u64 kernel_sp; // 内核的sp指针
} Trapframe;

typedef struct RawTrapFrame {
	u64 ra;
	u64 sp;
	u64 gp;
	u64 tp;
	u64 t0;
	u64 t1;
	u64 t2;
	u64 s0;
	u64 s1;
	u64 a0;
	u64 a1;
	u64 a2;
	u64 a3;
	u64 a4;
	u64 a5;
	u64 a6;
	u64 a7;
	u64 s2;
	u64 s3;
	u64 s4;
	u64 s5;
	u64 s6;
	u64 s7;
	u64 s8;
	u64 s9;
	u64 s10;
	u64 s11;
	u64 t3;
	u64 t4;
	u64 t5;
	u64 t6;
} RawTrapFrame;

enum ProcState { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

#define MAX_PROC_NAME_LEN 16
struct Proc;

LIST_HEAD(ProcList, Proc);
TAILQ_HEAD(ProcSchedQueue, Proc);
extern struct ProcList procFreeList;
extern struct ProcSchedQueue procSchedQueue[NCPU];

#define MAX_FD_COUNT 256

/**
 * zrp注：当前没有实现内核线程，每个用户进程只有用户态一种状态，难以处理
 * 睡眠问题。目前的策略是由唤醒者帮助睡眠者完成其事务，但这需要在进程控制块中
 * 存入较多的数据，并且耦合度较高。
 *
 * 之后会考虑实现内核线程。
 */
// 进程控制块
struct Proc {
	struct spinlock lock;

	// p->lock must be held when using these:
	enum ProcState state; // Process state
	char sleepReason[16]; // 睡眠原因
	/**
	 * 有下面几种情况：
	 * nanosleep：线程睡眠
	 * wait：等待子进程
	 *
	 */
	int killed; // If non-zero, have been killed
	int xstate; // Exit status to be returned to parent's wait
	u64 pid; // 进程ID，应当由进程在队列中的位置和累积创建进程排名组成

	// wait_lock must be held when using this:
	u64 parentId; // Parent process
	u64 priority;

	int fdList[MAX_FD_COUNT];

	// these are private to the process, so p->lock need not be held.
	uint64 sz;	  // Size of process memory (bytes)
	Pte *pageTable;	  // User page table
	u64 programBreak; // 用于brk系统调用，以增减该进程的堆空间
	// 进程可以访问任何处于programBreak及以下的内存

	struct ProcTime {
		u64 totalUtime; // 进程运行的总时钟数
		u64 lastTime;	// 进程上一次运行时的时钟数
		u64 totalStime; // 系统时间（可以实现为系统调用花掉的时间）

		u64 procSleepClocks; // 进程要睡眠的周期数
		u64 procSleepBegin;  // 进程开始睡眠的时间
	} procTime;

	// 实现等待机制的结构体
	struct Wait {
		u64 pid;
		u64 uPtr_status; // int *
		int options;
		u8 exitCode; // 进程的退出状态
	} wait;

	struct trapframe *trapframe;  // data page for trampoline.S
	struct file *ofile[NOFILE];   // Open files
	struct Dirent *cwd;	      // Current directory
	char name[MAX_PROC_NAME_LEN]; // Process name (debugging)

	LIST_ENTRY(Proc) procFreeLink;	// 空闲链表链接
	LIST_ENTRY(Proc) procSleepLink; // 进程睡眠链接(进程可以因为多种原因睡眠)
	LIST_ENTRY(Proc) procChildLink; // 子进程列表链接
	TAILQ_ENTRY(Proc) procSchedLink[NCPU]; // cpu调度队列链接
	struct ProcList childList;	       // 子进程列表
};

int cpuid();
struct cpu *mycpu(void);
struct Proc *myProc();

void procInit();
struct Proc *pidToProcess(u64 pid);
struct Proc *procCreate(const char *name, const void *binary, size_t size, u64 priority);
void procRun(struct Proc *prev, struct Proc *next);
void procDestroy(struct Proc *proc);
void procFree(struct Proc *proc);
int procFork(u64 stackTop);
void procExecve(char *path, u64 argv, u64 envp);

inline int procCanRun(struct Proc *proc) {
	return (proc->state == RUNNABLE || proc->state == RUNNING);
}

#define PROCESS_INIT 0x0400ul

// #symbol 可以将symbol原封不动地转换为对应的字符串（即两边加引号）
#define PROC_CREATE(programName, priority)                                                         \
	({                                                                                         \
		extern char binary_##programName[];                                                \
		extern int binary_##programName##_size;                                            \
		procCreate(#programName, binary_##programName, binary_##programName##_size,        \
			   priority);                                                              \
	})

#endif
