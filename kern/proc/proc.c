#include "proc/proc.h"
#include "defs.h"
#include "mm/memlayout.h"
#include "mm/memory.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include <lib/elf.h>
#include <lib/error.h>
#include <lib/string.h>
#include <trap/trap.h>

struct cpu cpus[NCPU];
extern char trampoline[];

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
// 下面的测试程序仅包含两条指令：j spin; nop
u8 initcode[] = {0x01, 0xa0, 0x01, 0x00};
// deprecated
void testProcRun() {
	log("start init...\n");
	struct cpu *c = mycpu();

	// 1. 设置proc
	testProc.state = RUNNABLE;
	strncpy(testProc.name, "init", 16);

	// 2. 分配页表
	Pte *pgDir = (Pte *)pageAlloc(); // TODO: ref += 1
	// bug: 自映射？你这个TODO的 ref++ 应该由 pageInsert() 添加自映射的时候来做
	testProc.pageTable = pgDir;

	// 3. 内存映射
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
	testProc.trapframe->sp = PAGE_SIZE;

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
 * @note 需要配置Trampoline、Trapframe、栈、页表（自映射）的映射
 * @param proc 进程指针
 * @return int <0表示出错
 */
int initProcPageTable(struct Proc *proc) {
	// 分配页表
	proc->pageTable = (Pte *)pageAlloc();

	// TRAMPOLINE
	TRY(ptMap(proc->pageTable, TRAMPOLINE, (u64)trampoline, PTE_R | PTE_X));

	// 该进程的trapframe
	proc->trapframe = (struct trapframe *)pageAlloc();
	TRY(ptMap(proc->pageTable, TRAPFRAME, (u64)proc->trapframe, PTE_R | PTE_W));

	// 该进程的栈
	u64 stack = pageAlloc();
	TRY(ptMap(proc->pageTable, USTACKTOP - PAGE_SIZE, stack, PTE_R | PTE_W | PTE_U));
	proc->trapframe->sp = USTACKTOP;

	// TODO：页表自映射

	return 0;
}

/**
 * @brief 分配一个进程，将其从空闲进程结构体中删除
 * @param pproc 所需要写入的进程proc指针的指针
 * @return int 错误类型
 */
static int procAlloc(struct Proc **pproc, u64 parentId) {
	struct Proc *proc = LIST_FIRST(&procFreeList);
	if (proc == NULL) {
		// 没有可分配的进程
		return -E_NOPROC;
	}

	assert(proc->state == UNUSED);

	TRY(initProcPageTable(proc));
	proc->parentId = parentId;

	*pproc = proc;
	return 0;
}

static int loadCodeMapper(void *data, u64 va, size_t offset, u64 perm, const void *src,
			  size_t len) {
	struct Proc *proc = (struct Proc *)data;

	// Step1: 分配一个页
	u64 pa = pageAlloc();

	// Step2: 复制段内数据
	if (src != NULL) {
		memcpy((void *)pa + offset, src, len);
	}

	// Step3: 将页pa插入到proc的页表中
	return pageInsert(proc->pageTable, va, pa, perm);
}

/**
 * @brief 加载ELF文件的各个段到指定的位置
 * @attention 你需要保证proc->trapframe已经设置完毕
 */
static int loadCode(struct Proc *proc, const void *binary, size_t size) {
	const ElfHeader *elfHeader = getElfFrom(binary, size);

	// 1. 判断ELF文件是否合法
	if (elfHeader == NULL) {
		return -E_BAD_ELF;
	}

	// 2. 加载文件到指定的虚拟地址
	size_t phOff;
	ELF_FOREACH_PHDR_OFF (phOff, elfHeader) {
		ProgramHeader *ph = (ProgramHeader *)(binary + phOff);
		// 只加载能加载的段
		if (ph->p_type == ELF_PROG_LOAD) {
			panic_on(loadElfSegment(ph, binary + ph->p_off, loadCodeMapper, proc));
		}
	}

	// 设置代码入口点
	proc->trapframe->epc = elfHeader->e_entry;
	return 0;
}

/**
 * @brief 创建一个进程，并加载一些必要的段(.text，.data)。该函数决定进程被加载到哪个CPU上
 *
 */
struct Proc *procCreate(const void *binary, size_t size, u64 priority) {
	struct Proc *proc;

	// 1. 申请一个Proc
	panic_on(procAlloc(&proc, 0));

	// 2. 设置进程优先级
	proc->priority = priority;
	proc->state = RUNNABLE;

	// 3. 加载进程的代码段和数据段
	panic_on(loadCode(proc, binary, size));

	// 4. 寻找一个合适的CPU插入进程
	int cpu = cpuid(); // TODO: 考虑随机分配的方法
	TAILQ_INSERT_HEAD(&procSchedQueue[cpu], proc, procSchedLink[cpu]);

	return proc;
}

/**
 * @brief 运行一个进程
 * @details 将该进程挂载到本cpu上（cpu->Proc = process），之后调用userTrapReturn
 *
 */
void procRun(struct Proc *proc) {
	mycpu()->Proc = proc;
	proc->state = RUNNING;
	userTrapReturn();
}

/**
 * @brief 结束一个进程
 *
 */
void procDestroy() {
}
