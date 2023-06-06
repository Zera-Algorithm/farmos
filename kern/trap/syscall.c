#include <dev/rtc.h>
#include <dev/sbi.h>
#include <dev/timer.h>
#include <fs/fd.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <proc/proc.h>
#include <proc/schedule.h>
#include <proc/sleep.h>
#include <proc/wait.h>
#include <trap/syscall_ids.h>

#define SYSCALL_ERROR -1

struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

struct tms {
	uint64 tms_utime;
	uint64 tms_stime;
	uint64 tms_cutime;
	uint64 tms_cstime;
};

struct timespec {
	uint64 second;
	long usec;
};

i64 sysMunmap(u64 start, u64 len);

int sysOpenat(int fd, u64 filename, int flags, mode_t mode) {
	return openat(fd, filename, flags, mode);
}

int sysClose(int fd) {
	return closeFd(fd);
}

int sysRead(int fd, u64 buf, size_t count) {
	return read(fd, buf, count);
}

#define STDIN 0
#define STDOUT 1
#define STDERR 2

// TODO: 注意：往stdout输出不能简单地理解为往控制台输出。实际上
// 有可能进程使用dup转移到另一个fd，向那个fd输出。
// static char strBuf[4096];
int sysWrite(int fd, u64 buf, size_t count) {
	// if (fd == STDOUT) {
	// 	assert(count < 4096);
	// 	copyIn((u64)buf, strBuf, count);
	// 	strBuf[count] = 0;
	// 	printf("%s", strBuf);
	// 	// warn("BUG PRINTF\n");
	// 	return count;
	// } else {
	return write(fd, buf, count);
	// }
}

int sysDup(int fd) {
	return dup(fd);
}

int sysDup3(int old, int new) {
	return dup3(old, new);
}

void sysShutdown() {
	SBI_SYSTEM_RESET(0, 0);
}

int sysGetCwd(u64 buf, int size) {
	char kBuf[256];
	fileGetPath(myProc()->cwd, kBuf);
	copyOut(buf, kBuf, strlen(kBuf) + 1);
	return buf;
}

/**
 * @brief 创建管道
 * @param pfd 指向int fd[2]的指针，存储返回的管道文件描述符。其中，fd[0]为读取，fd[1]为写入
 */
int sysPipe2(u64 pfd) {
	int fd[2];
	int ret = pipe(fd);
	if (ret < 0) {
		return ret;
	} else {
		copyOut(pfd, fd, sizeof(fd));
		return ret;
	}
}

int sysChdir(u64 path) {
	char kbuf[MAX_NAME_LEN];
	Dirent *newCwd;
	copyInStr(path, kbuf, MAX_NAME_LEN);

	// 绝对路径
	if (kbuf[0] == '/') {
		newCwd = getFile(NULL, kbuf);
	} else {
		newCwd = getFile(myProc()->cwd, kbuf);
	}
	if (newCwd == NULL) {
		warn("The new cwd is not found!\n");
		return -1;
	}
	myProc()->cwd = newCwd;
	return 0;
}

int sysMkDirAt(int dirFd, u64 path, int mode) {
	return makeDirAtFd(dirFd, path, mode);
}

int sysMount(u64 special, u64 dir, u64 fstype, u64 flags, u64 data) {
	// TODO
	return 0;
}

int sysUnMount(u64 special, u64 flags) {
	// TODO
	return 0;
}

int sysLinkAt(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags) {
	return linkAtFd(oldFd, pOldPath, newFd, pNewPath, flags);
}

int sysUnLinkAt(int dirFd, u64 pPath) {
	return unLinkAtFd(dirFd, pPath);
}

// brk如果输入0，则返回当前的program Break
i64 sysBrk(u64 addr) {
	struct Proc *proc = myProc();
	u64 oldBreak = proc->programBreak;
	if (addr == 0) {
		return oldBreak;
	} else {
		proc->programBreak = addr;

		// 如果要减小programBreak，就要unmap addr ~ oldBreak之间的内存
		if (addr < oldBreak) {
			return sysMunmap(addr + 1, oldBreak);
		}
		// 如果大于等于的话，则不做任何事情
		return 0;
	}
}

// Mmap 的一些宏定义
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0X02
#define MAP_FAILED ((void *)-1)

/**
 * @brief 将文件映射到进程的虚拟内存空间
 * @note 如果start == 0，则由内核指定虚拟地址
 */
void *sysMmap(u64 start, size_t len, int prot, int flags, int fd, off_t off) {
	int r = 0, perm = 0;
	Dirent *file;
	r = getDirentByFd(fd, &file, NULL);
	if (r < 0) {
		warn("get fd(%d) error!\n", fd);
		return MAP_FAILED;
	}

	if (start == 0) {
		// 内核指定用户的虚拟地址
		// TODO: 指定固定的地址有多次mmap被顶替的风险
		start = 0x60000000;
	}

	perm = PTE_U;
	if (prot & PROT_EXEC) {
		perm |= PTE_X;
	}
	if (prot & PROT_READ) {
		perm |= PTE_R;
	}
	if (prot & PROT_WRITE) {
		perm |= PTE_W;
	}

	// TODO: 需要考虑flags字段，但暂未考虑

	return mapFile(myProc(), file, start, len, perm, off);
}

i64 sysMunmap(u64 start, u64 len) {
	u64 from = PGROUNDUP(start);
	u64 to = PGROUNDDOWN(start + len - 1);
	for (u64 va = from; va <= to; va += PAGE_SIZE) {
		// 释放虚拟地址所在的页
		panic_on(ptUnmap(myProc()->pageTable, va));
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
// static char strBuf[4096];
// u64 sysWrite(int fd, const void *buf, size_t count) {
// 	if (fd == STDOUT) {
// 		assert(count < 4096);
// 		copyIn((u64)buf, strBuf, count);
// 		strBuf[count] = 0;
// 		printf("%s", strBuf);
// 		// warn("BUG PRINTF\n");
// 	} else {
// 		return SYSCALL_ERROR;
// 		panic("unimplement fd");
// 	}
// 	return count;
// }

void sysSchedYield() {
	// 设置返回值为0
	struct Proc *proc = myProc();
	proc->trapframe->a0 = 0;
	schedule(1);
}

int sysGetDents64(int fd, u64 buf, int len) {
	return getdents64(fd, buf, len);
}

int sysFstat(int fd, u64 pkstat) {
	return fileStatFd(fd, pkstat);
}

// --------------------- 进程管理部分系统调用 --------------
/**
 * @brief 克隆一个子进程（或者子线程）
 * @param flags 克隆选项。SIGCHLD：克隆子进程；
 * @param stack 进程的栈顶
 * @param ptid 父线程id，ignored
 * @param tls TLS线程本地存储描述符，ignored
 * @param ctid 子线程id，ignored
 * @return 成功返回子进程的id，失败返回-1
 */
u64 sysClone(u64 flags, u64 stack, u64 ptid, u64 tls, u64 ctid) {
	log(LEVEL_GLOBAL, "clone a process.\n");
	return procFork(stack);
}

u64 sysExecve(u64 path, u64 argv, u64 envp) {
	char kbuf[MAX_NAME_LEN];
	copyInStr(path, kbuf, MAX_NAME_LEN);
	procExecve(kbuf, argv, envp);
	return 0;
}

/**
 * @brief 等待子进程改变状态（一般是等待进程结束）
 * @param pid 要等待的进程。若为-1，表示等待任意的子进程；否则等待特定的子进程
 * @param pStatus 用户的int *status指针。用来存储进程的状态信息
 * @param options
 * 选项。包括WUNTRACED(因信号而停止)、WCONTINUED(因收到SIGCONT而恢复的)、WNOHANG(立即返回，无阻塞)
 */
u64 sysWait4(u64 pid, u64 pStatus, int options) {
	return wait(myProc(), pid, pStatus, options);
}

void sysExit(int exitCode) {
	struct Proc *proc = myProc();
	// 设置退出码
	proc->wait.exitCode = exitCode;
	procDestroy(proc);
}

u64 sysGetPpid() {
	return myProc()->parentId;
}

u64 sysGetPid() {
	return myProc()->pid;
}

/**
 * @brief 获取当前的Unix时间戳
 */
u64 sysGetTimeOfDay(u64 pTimeSpec) {
	struct timespec timeSpec;
	u64 usec = rtcReadTime();
	timeSpec.second = usec / 1000000;
	timeSpec.usec = usec % 1000000;
	copyOut(pTimeSpec, &timeSpec, sizeof(struct timespec));
	return 0;
}

/**
 * @brief 执行线程睡眠
 * @param pTimeSpec 包含秒和微秒两个字段，指明进程要睡眠的时间数
 */
void sysNanoSleep(u64 pTimeSpec) {
	struct timespec timeSpec;
	copyIn(pTimeSpec, &timeSpec, sizeof(timeSpec));
	u64 usec = timeSpec.second * 1000000 + timeSpec.usec;
	u64 clocks = usec * CLOCK_PER_USEC;
	log(LEVEL_MODULE, "time to wait: %d clocks\n", clocks);

	// 设置返回值
	myProc()->trapframe->a0 = 0;

	sleepProc(myProc(), clocks);
}

static void *syscallTable[] = {
    [SYS_brk] = sysBrk,
    [SYS_mmap] = sysMmap,
    [SYS_munmap] = sysMunmap,
    [SYS_times] = sysTimes,
    [SYS_uname] = sysUname,
    [SYS_read] = sysRead,
    [SYS_openat] = sysOpenat,
    [SYS_close] = sysClose,
    [SYS_write] = sysWrite,
    [SYS_dup] = sysDup,
    [SYS_dup3] = sysDup3,
    [SYS_sched_yield] = sysSchedYield,
    [SYS_clone] = sysClone,
    [SYS_execve] = sysExecve,
    [SYS_wait4] = sysWait4,
    [SYS_exit] = sysExit,
    [SYS_getppid] = sysGetPpid,
    [SYS_getpid] = sysGetPid,
    [SYS_gettimeofday] = sysGetTimeOfDay,
    [SYS_nanosleep] = sysNanoSleep,
    [SYS_getcwd] = sysGetCwd,
    [SYS_chdir] = sysChdir,
    [SYS_getdents64] = sysGetDents64,
    [SYS_shutdown] = sysShutdown,
    [SYS_mkdirat] = sysMkDirAt,
    [SYS_mount] = sysMount,
    [SYS_umount2] = sysUnMount,
    [SYS_linkat] = sysLinkAt,
    [SYS_unlinkat] = sysUnLinkAt,
    [SYS_fstat] = sysFstat,
    [SYS_pipe2] = sysPipe2,
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
		tf->a0 = SYSCALL_ERROR;
		warn("unimplemented or unknown syscall: %d\n", sysno);
		return;
	}

	// 将系统调用返回值放入a0寄存器
	tf->a0 = func(tf->a0, tf->a1, tf->a2, tf->a3, tf->a4, tf->a5);

	// S态时间审计
	u64 endTime = getTime();
	myProc()->procTime.totalStime += (endTime - startTime);
}
