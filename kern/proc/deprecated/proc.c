#include <dev/timer.h>
#include <fs/cluster.h>
#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/elf.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmtools.h>
#include <param.h>
#include <proc/proc.h>
#include <proc/schedule.h>
#include <proc/wait.h>
#include <riscv.h>
#include <trap/trap.h>
#include <types.h>

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

static void dupPage(Pte *childTable, Pte *procTable, u64 va) {
	Pte pte = ptLookup(procTable, va);
	assert(pte != 0);
	u64 perm = PTE_PERM(pte);

	log(LEVEL_GLOBAL, "dupPage va = 0x%08lx\n", va);
	// 1. 如果该页不可写，或是PTE_COW的，那么就原样映射
	if ((pte & PTE_W) == 0 || (pte & PTE_COW)) {
		panic_on(ptMap(childTable, va, pteToPa(pte), perm));
	}
	// 2. 否则，两者都加上PTE_COW位再映射
	else {
		panic_on(ptMap(childTable, va, pteToPa(pte), (perm ^ PTE_W) | PTE_COW));
		panic_on(ptMap(procTable, va, pteToPa(pte), (perm ^ PTE_W) | PTE_COW));
	}
}

static err_t dupPageCallback(Pte *pd, u64 target_va, Pte *target_pte, void *arg) {
	Pte *childPd = (Pte *)arg;
	// 查询child是否在此页有map，如果有，就跳过该页
	Pte pte = ptLookup(childPd, target_va);
	if (pte != 0 && (pte & PTE_U) == 0) {
		log(DEBUG, "skip va = 0x%016lx, pte = 0x%016lx\n", target_va, pte);
		return 0;
	}
	log(DEBUG, "try to dup va = 0x%016lx\n", target_va);
	dupPage(childPd, pd, target_va);
	return 0;
}

/**
 * @brief 产生一个子进程。如果stackTop不为0，则设其为新进程的栈顶，否则沿用之前进程的栈
 */
int procFork(u64 stackTop) {
	struct Proc *proc = myProc();
	struct Proc *child;

	log(DEBUG, "begin fork!\n");

	// 1. 申请一个proc
	panic_on(procAlloc(&child, proc->pid, stackTop));

	log(DEBUG, "alloc a proc!\n");

	// 2. 设置进程信息
	child->priority = proc->priority;
	child->state = RUNNABLE; // 等待下一步执行
	strncpy(child->name, proc->name, MAX_PROC_NAME_LEN);
	proc->name[MAX_PROC_NAME_LEN - 1] = 0;

	log(DEBUG, "begin scan page tables!\n");

	// 3. COW 进程的未写入的页
	pdWalk(proc->pageTable, dupPageCallback, NULL, child->pageTable);

	log(DEBUG, "end scan page Table!\n");

	// 4. 复制寄存器的值（除了sp）
	*(child->trapframe) = *(proc->trapframe);
	if (stackTop) {
		child->trapframe->sp = stackTop;
	}
	child->trapframe->a0 = 0; // 为子进程准备返回值

	// Note: 复制父进程已打开的文件
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		int kFd = proc->fdList[i];
		if (kFd != -1) {
			cloneAddCite(kFd);
		}
		child->fdList[i] = kFd;
	}

	// 5. 寻找一个合适的CPU插入进程
	int cpu = cpuid(); // TODO: 考虑随机分配的方法
	log(DEBUG, "insert child proc %08lx\n", child->pid);
	TAILQ_INSERT_HEAD(&procSchedQueue[cpu], child, procSchedLink[cpu]);
	assert(!TAILQ_EMPTY(&procSchedQueue[cpu]));

	// 父进程返回子进程的pid，实现一个函数，两个返回值
	log(DEBUG, "end fork!\n");
	return child->pid;
}

// TODO: envp
void procExecve(char *path, u64 argv, u64 envp) {
	void *binary;
	int size;

	// 要读取的文件
	Dirent *file = getFile(myProc()->cwd, path);
	loadFileToKernel(file, &binary, &size);

	struct Proc *proc = myProc();

	strncpy(proc->name, path, 16);

	// 1. 初始化trapframe
	memset(proc->trapframe, 0, sizeof(struct trapframe));

	// 2. 重新分配栈
	// 之所以放在加载进程的代码段和数据段之前，是为了避免进程之前输入的字符串数据被覆盖
	u64 stack = vmAlloc();
	proc->trapframe->sp = initStack((void *)(stack + PAGE_SIZE), argv);
	panic_on(ptMap(proc->pageTable, USTACKTOP - PAGE_SIZE, stack, PTE_R | PTE_W | PTE_U));
	log(LEVEL_MODULE, "end init stack!\n");

	// 3. 加载进程的代码段和数据段
	log(LEVEL_GLOBAL, "Execve file %s\n", path);
	panic_on(loadCode(proc, binary, size, &proc->programBreak));
}

/**
 * @brief 分配一个进程，将其从空闲进程结构体中删除
 * @param pproc 所需要写入的进程proc指针的指针
 * @param parentId 父进程pid
 * @param stack 栈顶的位置（为0表示默认）
 * @return int 错误类型
 */
static int procAlloc(struct Proc **pproc, u64 parentId, u64 stackTop) {
	log(DEBUG, "begin procAlloc!\n");
	// 1. 寻找可分配的进程控制块
	struct Proc *proc = LIST_FIRST(&procFreeList);

	log(DEBUG, "we get proc %lx\n", proc);

	if (proc == NULL) {
		// 没有可分配的进程
		return -E_NOPROC;
	}

	log(DEBUG, "we get proc %lx(verified)\n", proc);

	assert(proc->state == UNUSED);

	log(DEBUG, "before delete %lx(verified)\n", proc);

	// 2. 从空闲链表中删除此进程控制块
	LIST_REMOVE(proc, procFreeLink);

	log(DEBUG, "before init pageTable!\n");

	// 3. 初始化进程proc的页表
	unwrap(initProcPageTable(proc, stackTop));

	log(DEBUG, "after init pageTable!\n");

	// 4. 设置进程的pid和父亲id，初始化子进程列表
	proc->pid = makeProcId(proc);
	proc->parentId = parentId;
	LIST_INIT(&proc->childList);

	// 5. 如果parentId不为0，那么就将该进程加入到parentId进程的子进程列表中
	if (parentId != 0) {
		struct Proc *parent = pidToProcess(parentId);
		assert(parent != NULL);
		LIST_INSERT_HEAD(&(parent->childList), proc, procChildLink);
	}

	// 6. 初始化进程的文件描述符表
	// -1 表示未分配
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		proc->fdList[i] = -1;
	}
	proc->fdList[0] = readConsoleAlloc();
	proc->fdList[1] = writeConsoleAlloc();
	proc->fdList[2] = errorConsoleAlloc();

	// 7. 设置工作目录
	extern FileSystem *fatFs;
	proc->cwd = &fatFs->root;

	// 8. 返回分配的进程控制块
	*pproc = proc;
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
	static int creations = 0;

	creations += 1;
	struct Proc *proc;

	// 1. 申请一个Proc
	// 设置父进程
	u64 parentId = (creations == 1) ? 0 : PROCESS_INIT;
	panic_on(procAlloc(&proc, parentId, 0));

	// 2. 设置进程信息
	proc->priority = priority;
	proc->state = RUNNABLE;
	strncpy(proc->name, name, MAX_PROC_NAME_LEN);
	proc->name[MAX_PROC_NAME_LEN - 1] = 0; // 防止缓冲区溢出

	// 3. 加载进程的代码段和数据段
	panic_on(loadCode(proc, binary, size, &proc->programBreak));

	// 4. 寻找一个合适的CPU插入进程
	int cpu = cpu_this_id(); // TODO: 考虑随机分配的方法
	log(DEFAULT, "insert proc %08lx\n", proc->pid);
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
	// log(LEVEL_GLOBAL, "%03d:  %8s(0x%08lx)\n", ++order, next->name, next->pid);

	mycpu()->proc = next;
	next->state = RUNNING;
	utrap_return();
}

static void freeProcFds(struct Proc *proc) {
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		if (proc->fdList[i] != -1) {
			freeFd(proc->fdList[i]);
		}
	}
}

/**
 * @brief 杀死进程，回收一个进程控制块的大部分资源。包括回收进程页表映射的物理内存、回收页表存储等
 * 		  将进程变为僵尸进程
 *
 */
static void killProc(struct Proc *proc) {
	log(DEFAULT, "kill proc %s(0x%08lx)\n", proc->name, proc->pid);

	// 1. 遍历进程页目录并回收
	pdWalk(proc->pageTable, vmUnmapper, kvmUnmapper, NULL);

	// 2. 回收进程的文件描述符
	freeProcFds(proc);

	// 3. 清空进程控制块中的页表域
	proc->pageTable = 0;

	// 4. 从调度队列中删除
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

	// 5. 将进程变为僵尸进程
	proc->state = ZOMBIE;
}

/**
 * @brief 回收僵尸进程的进程控制块
 */
void procFree(struct Proc *proc) {
	log(LEVEL_GLOBAL, "trigger procFree! %d\n", proc - procs);
	assert(proc->state == ZOMBIE);

	// 1. 从父进程的子进程列表删除
	LIST_REMOVE(proc, procChildLink);

	// 2. 清空进程控制块
	memset(proc, 0, sizeof(struct Proc));

	// 3. 插入到空闲链表
	LIST_INSERT_HEAD(&procFreeList, proc, procFreeLink);
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
	log(LEVEL_GLOBAL, "I will kill proc %s(0x%08lx)\n", proc->name, proc->pid);
	// 杀死进程
	killProc(proc);

	// 处理子进程
	doChildManage(proc);

	// 尝试唤醒父进程
	tryWakeupParentProc(proc);

	if (myProc() == proc) {
		mycpu()->proc = NULL;
		log(LEVEL_GLOBAL, "cpu %d's running process has been killed.\n", cpuid());
		schedule(1);
	}
}
