#include "proc/proc.h"
#include "defs.h"
#include "mm/memlayout.h"
#include "mm/memory.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include <lib/error.h>
#include <lib/string.h>
#include <trap/trap.h>

struct cpu cpus[NCPU];

int cpuid() {
	int id = r_tp();
	return id;
}

struct cpu *mycpu(void) {
	int id = cpuid();
	return (&cpus[id]);
}

struct Proc *myProc() {
	// TODO: 按照xv6的代码，可能需要开关中断？
	struct cpu *c = mycpu();
	struct Proc *p = c->Proc;
	return p;
}

/**
 * @brief 测试版的procRun，运行一个进程
 * @note 此处与MOS不同。MOS是通过位于内核态的时钟中断来激活第一个进程的。
 * 		而我们的内核用户态时钟中断是与内核态时钟中断分开的，
 * 		调度只能在用户态的时钟中断中实现。所以需要手动运行第一个进程
 */
struct Proc testProc;
u8 initcode[] = {0x01, 0xa0, 0x01, 0x00};
void testProcRun() {
	log("start init...\n");
	struct cpu *c = mycpu();

	// 1. 设置proc
	testProc.state = RUNNABLE;
	strncpy(testProc.name, "init", 16);

	// 2. 分配页表
	Pte *pgDir = (Pte *)pageAlloc(); // TODO: ref += 1
	// bug: 自映射？你这个TODO的 ref++ 应该由 pageInsert() 添加自映射的时候来做
	testProc.pagetable = pgDir;

	// 3. 内存映射
	extern char trampoline[];
	pageInsert(pgDir, TRAMPOLINE, PGROUNDDOWN((u64)trampoline), PTE_R | PTE_X);

	log("to Alloc a page\n");
	void *trapframe = (void *)pageAlloc();
	pageInsert(pgDir, TRAPFRAME, (u64)trapframe, PTE_R | PTE_W);
	assert(pteToPa(pageLookup(pgDir, TRAPFRAME)) == (u64)trapframe);

	testProc.trapframe = trapframe;

	// 4. 映射代码段
	void *code = (void *)pageAlloc();
	memcpy(code, initcode, sizeof(initcode));
	pageInsert(pgDir, 0, (u64)code, PTE_R | PTE_X | PTE_U); // 需要设置PTE_U允许用户访问
	assert(pteToPa(pageLookup(pgDir, 0)) == (u64)code);

	// 5. 设置Trapframe
	testProc.trapframe->epc = 0;
	testProc.trapframe->sp = PGSIZE;

	c->Proc = &testProc;
	userTrapReturn();
}

extern struct Proc procs[];
/**
 * @brief 为进程分配一个新的pid
 * @param proc 要分配pid的proc结构体
 * @details [..54..|..10..]
 * @return u64 进程pid
 */
static u64 makeProcId(struct Proc *proc) {
	static int cnt = 0;
	return (proc - procs) | ((++cnt) * NPROC);
}

/**
 * @brief 进程id到process的转换
 * @param pid 进程pid
 * @return struct Proc *：pid对应的进程。成功返回Proc指针，失败返回NULL
 */
struct Proc *pidToProcess(u64 pid) {
	struct Proc *proc = &procs[pid & (NPROC - 1)];
	if (proc->pid != pid || proc->state == UNUSED) {
		return NULL;
	}
	// TODO: 可能需要做一些权限的判断
	return proc;
}

/**
 * @brief 初始化proc的页表
 * @note 需要配置代码段、数据段、栈、Trampoline、Trapframe、页表（自映射）的映射
 * @param proc 进程指针
 * @return int <0表示出错
 */
int initProcPageTable(struct Proc *proc) {
	// TODO
	return 0;
}

/**
 * @brief 分配一个进程，将其从空闲进程结构体中删除
 * @param pproc 所需要写入的进程proc指针的指针
 * @return int 错误类型
 */
static int procAlloc(struct Proc **pproc) {
	struct Proc *proc = LIST_FIRST(&procFreeList);
	if (proc == NULL) {
		// 没有可分配的进程
		return -E_NOPROC;
	}

	assert(proc->state == UNUSED);

	// 为此进程写入信息
	proc->trapframe = (struct trapframe *)pageAlloc();

	TRY(initProcPageTable(proc));
	*pproc = proc;
	return 0;
}

/**
 * @brief 创建一个进程，并加载一些必要的段(.text，.data)
 *
 */
void procCreate() {
}

/**
 * @brief 运行一个进程
 * @details 将该进程挂载到本cpu上（cpu->Proc = process），之后调用userTrapReturn
 *
 */
void procRun() {
}

/**
 * @brief 结束一个进程
 *
 */
void procDestroy() {
}
