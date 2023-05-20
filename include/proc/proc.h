#ifndef _PROC_H
#define _PROC_H

#include "lock/spinlock.h"
#include "param.h"
#include "riscv.h"
#include <lib/queue.h>
#include <mm/memory.h>

// Per-CPU state.
struct cpu {
	struct Proc *Proc; // The process running on this cpu, or null.
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
struct trapframe {
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
};

enum ProcState { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct Proc;

// Per-process state
struct Proc {
	struct spinlock lock;

	// p->lock must be held when using these:
	enum ProcState state; // Process state
	void *chan;	      // If non-zero, sleeping on chan
	int killed;	      // If non-zero, have been killed
	int xstate;	      // Exit status to be returned to parent's wait
	u64 pid; // 进程ID，应当由进程在队列中的位置和累积创建进程排名组成

	// wait_lock must be held when using this:
	struct Proc *parent; // Parent process

	// these are private to the process, so p->lock need not be held.
	uint64 sz;		     // Size of process memory (bytes)
	Pte *pagetable;		     // User page table
	struct trapframe *trapframe; // data page for trampoline.S
	struct file *ofile[NOFILE];  // Open files
	struct inode *cwd;	     // Current directory
	char name[16];		     // Process name (debugging)

	LIST_ENTRY(Proc) procFreeLink;	       // 空闲链表链接
	TAILQ_ENTRY(Proc) procSchedLink[NCPU]; // cpu调度队列链接
};

LIST_HEAD(ProcFreeList, Proc);
TAILQ_HEAD(ProcSchedQueue, Proc);
extern struct ProcFreeList procFreeList;
extern struct ProcSchedQueue procSchedQueue[NCPU];

int cpuid();
struct cpu *mycpu(void);
struct Proc *myProc();

#endif
