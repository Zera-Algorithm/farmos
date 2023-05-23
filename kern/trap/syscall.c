#include <dev/timer.h>
#include <lib/string.h>
#include <proc/proc.h>
#include <proc/schedule.h>
#include <proc/sleep.h>
#include <trap/syscallDataStruct.h>
#include <trap/syscall_ids.h>

i64 sysMunmap(u64 start, u64 len);
void sysSchedYield();

/**
 * @brief 将内核的数据拷贝到用户态地址
 */
void copyOut(u64 uPtr, void *kPtr, int len) {
	Pte *pgDir = myProc()->pageTable;

	void *p = (void *)pteToPa(ptLookup(pgDir, uPtr));
	u64 offset = uPtr & (PAGE_SIZE - 1);

	if (offset != 0) {
		memcpy(p + offset, kPtr, MIN(len, PAGE_SIZE - offset));
	}

	u64 i = uPtr + MIN(len, PAGE_SIZE - offset);
	u64 dstVa = uPtr + len;
	for (; i < dstVa; i += PAGE_SIZE) {
		p = (void *)pteToPa(ptLookup(pgDir, i));
		memcpy(p, kPtr + i - uPtr, MIN(dstVa - i, PAGE_SIZE));
	}
}

/**
 * @brief 将用户的数据拷贝入内核
 */
void copyIn(u64 uPtr, void *kPtr, int len) {
	Pte *pgDir = myProc()->pageTable;

	void *p = (void *)pteToPa(ptLookup(pgDir, uPtr));
	u64 offset = uPtr & (PAGE_SIZE - 1);

	if (offset != 0) {
		memcpy(kPtr, p + offset, MIN(len, PAGE_SIZE - offset));
	}

	u64 i = uPtr + MIN(len, PAGE_SIZE - offset);
	u64 dstVa = uPtr + len;
	for (; i < dstVa; i += PAGE_SIZE) {
		p = (void *)pteToPa(ptLookup(pgDir, i));
		memcpy(kPtr + i - uPtr, p, MIN(dstVa - i, PAGE_SIZE));
	}
}

i64 sysBrk(u64 addr) {
	struct Proc *proc = myProc();
	u64 oldBreak = proc->programBreak;
	proc->programBreak = addr;

	// 如果要减小programBreak，就要unmap addr ~ oldBreak之间的内存
	if (addr < oldBreak) {
		return sysMunmap(addr + 1, oldBreak);
	}
	return 0;
}

// 需要实现文件系统
i64 sysMmap(void *start, size_t len, int prot, int flags, int fd, off_t off) {
	panic("unimplemented");
}

i64 sysMunmap(u64 start, u64 len) {
	u64 from = PGROUNDUP(start);
	u64 to = PGROUNDDOWN(start + len - 1);
	for (u64 va = from; va <= to; va += PAGE_SIZE) {
		// 释放虚拟地址所在的页
		catchMemErr(pageRemove(myProc()->pageTable, va));
	}
	return 0;
}

i64 sysTimes(u64 pTms) {
	struct Proc *proc = myProc();
	struct tms tms;
	tms.tms_utime = proc->procTime.totalUtime;
	tms.tms_stime = proc->procTime.totalStime;

	// TODO: unimplement：父进程等待子进程的这一时间段内，子进程使用的CPU时间
	tms.tms_cstime = 0;
	tms.tms_cutime = 0;

	copyOut(pTms, &tms, sizeof(struct tms));
	return getTime();
}

i64 sysUname(u64 utsName) {
	struct utsname uname;

	strncpy(uname.sysname, "farm_os", 65);
	strncpy(uname.nodename, "my_node", 65);
	strncpy(uname.release, "1.0-alpha", 65);
	strncpy(uname.version, "1.0-alpha", 65);
	strncpy(uname.machine, "RISC-V Hifive Unmatched", 65);
	strncpy(uname.domainname, "Beihang", 65);

	copyOut(utsName, &uname, sizeof(struct utsname));
	return 0;
}

#define STDIN 0
#define STDOUT 1
#define STDERR 2

// TODO: 注意：往stdout输出不能简单地理解为往控制台输出。实际上
// 有可能进程使用dup转移到另一个fd，向那个fd输出。
static char strBuf[4096];
u64 sysWrite(int fd, const void *buf, size_t count) {
	if (fd == STDOUT) {
		assert(count < 4096);
		copyIn((u64)buf, strBuf, count);
		strBuf[count] = 0;
		printf("%s", strBuf);
	} else {
		panic("unimplement fd");
	}
	return count;
}

void sysSchedYield() {
	// 设置返回值为0
	struct Proc *proc = myProc();
	proc->trapframe->a0 = 0;
	schedule(1);
}

// --------------------- 进程管理部分系统调用 --------------
u64 sysClone() {
	panic("unimplemented");
	return 0;
}

u64 sysExecve() {
	panic("unimplemented");
	return 0;
}

u64 sysWait4() {
	panic("unimplemented");
	return 0;
}

void sysExit() {
	procDestroy(myProc());
}

u64 sysGetPpid() {
	return myProc()->parentId;
}

u64 sysGetPid() {
	return myProc()->pid;
}

u64 sysGetTimeOfDay(u64 pTimeSpec) {
	struct timespec timeSpec;
	u64 usec = getUSecs();
	timeSpec.second = usec / 1000000;
	timeSpec.usec = usec % 1000000;
	copyOut(pTimeSpec, &timeSpec, sizeof(struct timespec));
	return 0;
}

void sysNanoSleep(u64 pTimeSpec) {
	struct timespec timeSpec;
	copyIn(pTimeSpec, &timeSpec, sizeof(timeSpec));
	u64 usec = timeSpec.second * 1000000 + timeSpec.usec;
	u64 clocks = usec * CLOCK_PER_USEC;

	// 设置返回值
	myProc()->trapframe->a0 = 0;

	sleepProc(myProc(), clocks);

	mycpu()->proc = NULL; // 将此CPU上的进程取下来
	schedule(1);
}

static void *syscallTable[] = {
    [SYS_brk] = sysBrk,
    [SYS_mmap] = sysMmap,
    [SYS_munmap] = sysMunmap,
    [SYS_times] = sysTimes,
    [SYS_uname] = sysUname,
    [SYS_write] = sysWrite,
    [SYS_sched_yield] = sysSchedYield,
    [SYS_getpid] = sysGetPid,
    [SYS_getppid] = sysGetPpid,
    [SYS_exit] = sysExit,
    [SYS_wait4] = sysWait4,
    [SYS_execve] = sysExecve,
    [SYS_clone] = sysClone,
    [SYS_gettimeofday] = sysGetTimeOfDay,
    [SYS_nanosleep] = sysNanoSleep,
};

/**
 * @brief 系统调用入口。会按照tf中传的参数信息（a0~a7）调用相应的系统调用函数，并将返回值保存在a0中
 *
 */
void syscallEntry(Trapframe *tf) {
	// S态时间审计
	u64 startTime = getTime();

	u64 sysno = tf->a7;
	// 系统调用最多6个参数
	u64 (*func)(u64, u64, u64, u64, u64, u64);

	// 根据反汇编结果，一个ecall占用4字节的空间
	tf->epc += 4;

	// 获取系统调用函数
	func = (u64(*)(u64, u64, u64, u64, u64, u64))syscallTable[sysno];
	if (func == 0) {
		panic("no such syscall: %d\n", sysno);
	}

	// 将系统调用返回值放入a0寄存器
	tf->a0 = func(tf->a0, tf->a1, tf->a2, tf->a3, tf->a4, tf->a5);

	// S态时间审计
	u64 endTime = getTime();
	myProc()->procTime.totalStime += (endTime - startTime);
}
