#include <dev/timer.h>
#include <lib/elf.h>
#include <lib/error.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <mm/memlayout.h>
#include <mm/memory.h>
#include <param.h>
#include <proc/proc.h>
#include <proc/schedule.h>
#include <proc/wait.h>
#include <riscv.h>
#include <trap/trap.h>
#include <types.h>

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
	struct Proc *p = c->proc;
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
	loga("start init...\n");
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

	loga("to Alloc a page\n");
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

	c->proc = &testProc;
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
static int initProcPageTable(struct Proc *proc) {
	// 分配页表
	u64 pa = vmAlloc();
	proc->pageTable = (Pte *)pa;
	pmPageIncRef(paToPage(pa));

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
 * @param parentId 父进程pid
 * @return int 错误类型
 */
static int procAlloc(struct Proc **pproc, u64 parentId) {
	// 1. 寻找可分配的进程控制块
	struct Proc *proc = LIST_FIRST(&procFreeList);
	if (proc == NULL) {
		// 没有可分配的进程
		return -E_NOPROC;
	}

	assert(proc->state == UNUSED);

	// 2. 从空闲链表中删除此进程控制块
	LIST_REMOVE(proc, procFreeLink);

	// 3. 初始化进程proc的页表
	TRY(initProcPageTable(proc));

	// 4. 设置进程的pid和父亲id，初始化子进程列表
	proc->pid = makeProcId(proc);
	proc->parentId = parentId;
	LIST_INIT(&proc->childList);

	// 5. 返回分配的进程控制块
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
 * @param maxva 你分给进程的虚拟地址的最大值，用于计算最初的Program Break
 */
static int loadCode(struct Proc *proc, const void *binary, size_t size, u64 *maxva) {
	const ElfHeader *elfHeader = getElfFrom(binary, size);

	// 1. 判断ELF文件是否合法
	if (elfHeader == NULL) {
		return -E_BAD_ELF;
	}

	*maxva = 0;

	// 2. 加载文件到指定的虚拟地址
	size_t phOff;
	ELF_FOREACH_PHDR_OFF (phOff, elfHeader) {
		ProgramHeader *ph = (ProgramHeader *)(binary + phOff);
		// 只加载能加载的段
		if (ph->p_type == ELF_PROG_LOAD) {
			panic_on(loadElfSegment(ph, binary + ph->p_off, loadCodeMapper, proc));
			*maxva = MAX(*maxva, ph->p_vaddr + ph->p_memsz - 1);
		}
	}

	*maxva = PGROUNDUP(*maxva);

	// 设置代码入口点
	proc->trapframe->epc = elfHeader->e_entry;
	return 0;
}

/**
 * @brief 创建一个进程，并加载一些必要的段(.text，.data)。该函数决定进程被加载到哪个CPU上
 * @param binary 要加载的进程的二进制数据指针
 * @param size 二进制数据的大小
 * @param priority 进程的优先级
 * @param name 进程名称
 */
struct Proc *procCreate(const char *name, const void *binary, size_t size, u64 priority) {
	struct Proc *proc;

	// 1. 申请一个Proc
	panic_on(procAlloc(&proc, 0));

	// 设置父进程
	if (proc->pid != PROCESS_INIT) {
		proc->parentId = PROCESS_INIT;
	}

	// 2. 设置进程信息
	proc->priority = priority;
	proc->state = RUNNABLE;
	strncpy(proc->name, name, MAX_PROC_NAME_LEN);
	proc->name[MAX_PROC_NAME_LEN - 1] = 0; // 防止缓冲区溢出

	// 3. 加载进程的代码段和数据段
	panic_on(loadCode(proc, binary, size, &proc->programBreak));

	// 4. 寻找一个合适的CPU插入进程
	int cpu = cpuid(); // TODO: 考虑随机分配的方法
	loga("insert proc %08lx\n", proc->pid);
	TAILQ_INSERT_HEAD(&procSchedQueue[cpu], proc, procSchedLink[cpu]);
	assert(!TAILQ_EMPTY(&procSchedQueue[cpu]));

	return proc;
}

/**
 * @brief 审计进程运行的时间(utime)
 */
static void countProcTime(struct Proc *prev, struct Proc *next) {
	u64 curTime = getTime();

	// 防止prev为NULL
	if (prev) {
		u64 lastTime = curTime - prev->procTime.lastTime;
		prev->procTime.totalUtime += lastTime;
	}

	if (next) {
		next->procTime.lastTime = curTime;
	}
}

/**
 * @brief 运行一个进程
 * @details 将该进程挂载到本cpu上（cpu->Proc = process），之后调用userTrapReturn
 * @param proc 待运行的进程
 */
void procRun(struct Proc *prev, struct Proc *next) {
	countProcTime(prev, next);

	// static int order = 0;
	// // 打印当前调度的进程的信息
	// loga("%03d:  %8s(0x%08lx)\n", ++order, next->name, next->pid);

	mycpu()->proc = next;
	next->state = RUNNING;
	userTrapReturn();
}

/**
 * @brief 杀死进程，回收一个进程控制块的大部分资源。包括回收进程页表映射的物理内存、回收页表存储等
 * 		  将进程变为僵尸进程
 *
 */
static void killProc(struct Proc *proc) {
	loga("kill proc %s(0x%08lx)\n", proc->name, proc->pid);

	// 1. 回收页表
	for (u32 i = 0; i < PTX(~0, 1); i++) {
		if (!(proc->pageTable[i] & PTE_V)) {
			continue;
		}
		Pte *pt1 = (Pte *)pteToPa(proc->pageTable[i]);

		for (u32 j = 0; j < PTX(~0, 2); j++) {
			if (!(pt1[j] & PTE_V)) {
				continue;
			}
			Pte *pt2 = (Pte *)pteToPa(pt1[j]);

			for (u32 k = 0; k < PTX(~0, 3); k++) {
				if (!(pt2[k] & PTE_V)) {
					continue;
				}

				u64 va = (i << 30) | (j << 21) | (k << 12);
				ptUnmap(proc->pageTable, va);
			}

			// 清空二级页表项，回收三级页表
			pmPageDecRef(pteToPage(pt1[j]));
			((u64 *)pt1)[j] = 0;
		}

		// 清空一级页表项，回收二级页表
		pmPageDecRef(pteToPage(proc->pageTable[i]));
		((u64 *)proc->pageTable)[i] = 0;
	}

	// 回收一级页表（页目录）
	pmPageDecRef(paToPage((u64)proc->pageTable));
	proc->pageTable = 0;

	// 2. 从调度队列中删除
	assert(procCanRun(proc)); // 假定进程在运行队列中（如果不在的话，处理起来比较困难）
	// LIST_INSERT_HEAD(&procFreeList, proc, procFreeLink);
	for (int i = 0; i < NCPU; i++) {
		struct Proc *tmp;
		int isRemove = 0;
		TAILQ_FOREACH (tmp, &procSchedQueue[i], procSchedLink[i]) {
			if (tmp == proc) {
				TAILQ_REMOVE(&procSchedQueue[i], proc, procSchedLink[i]);
				isRemove = 1;
				break;
			}
		}
		if (isRemove) {
			break;
		}
	}

	// 3. 将进程变为僵尸进程
	proc->state = ZOMBIE;
}

/**
 * @brief 回收僵尸进程的进程控制块
 */
void procFree(struct Proc *proc) {
	assert(proc->state == ZOMBIE);

	// 1. 插入到空闲链表
	LIST_INSERT_HEAD(&procFreeList, proc, procFreeLink);

	// 2. 从父进程的子进程列表删除
	LIST_REMOVE(proc, procChildLink);

	// 3. 清空进程控制块
	memset(proc, 0, sizeof(struct Proc));
}

/**
 * @brief 在父进程结束后，处理子进程。
 *        若ZOMBIE，直接free；否则改变父亲为init
 */
static void doChildManage(struct Proc *parent) {
	struct Proc *proc, *tmp;
	struct Proc *init = pidToProcess(PROCESS_INIT);
	assert(init != NULL);

	LIST_FOREACH_DELETE(proc, &parent->childList) {
		tmp = proc;			       // 要删除的子进程
		proc = LIST_NEXT(proc, procChildLink); // 迭代

		if (tmp->state == ZOMBIE) {
			procFree(tmp); // 会从本链表中删除
		} else {
			// 改变父进程为init
			tmp->parentId = PROCESS_INIT;
			LIST_REMOVE(tmp, procChildLink);
			// 插入到INIT进程的子进程列表
			LIST_INSERT_HEAD(&init->childList, tmp, procChildLink);
		}
	}
}

/**
 * @brief 结束一个进程
 * @param proc 要结束的进程
 */
void procDestroy(struct Proc *proc) {
	// 杀死进程
	killProc(proc);

	// 处理子进程
	doChildManage(proc);

	// 尝试唤醒父进程
	tryWakeupParentProc(proc);

	if (myProc() == proc) {
		mycpu()->proc = NULL;
		loga("cpu %d's running process has been killed.\n", cpuid());
		schedule(1);
	}
}
