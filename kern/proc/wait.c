#include <lib/printf.h>
#include <lib/string.h>
#include <proc/proc.h>
#include <proc/sleep.h>
#include <proc/wait.h>

void copyOut(u64 uPtr, void *kPtr, int len);

// TODO: 需要实现WUNTRACED（被信号停止）、WCONTINUED（因信号恢复执行）
// 依赖：实现信号

inline static u64 __waitChild(struct Proc *proc, u64 pid, u64 pStatus, int options) {
	if (options & WNOHANG) {
		return 0;
	} else {
		proc->wait.pid = pid;
		proc->wait.uPtr_status = pStatus;
		proc->wait.options = options;
		naiveSleep(proc, "wait"); // 永不返回
		return 0;		  // 欺骗编译器
	}
}

/**
 * @brief 等待子进程改变状态（一般是等待进程结束）
 * @param proc 在等待的父进程
 * @param pid 要等待的进程。若为-1，表示等待任意的子进程；否则等待特定的子进程
 * @param pStatus 用户的int *status指针。用来存储进程的状态信息
 * @param options
 * 选项。包括WUNTRACED(因信号而停止)、WCONTINUED(因收到SIGCONT而恢复的)、WNOHANG(立即返回，无阻塞)
 */
u64 wait(struct Proc *proc, u64 pid, u64 pStatus, int options) {
	// pStatus要返回的数据
	union WaitStatus status;
	status.val = 0;

	if (pid != -1) {
		// 1. 寻找对应的子进程
		struct Proc *proc = pidToProcess(pid);
		if (proc == NULL) {
			return -1;
		}

		// 2. 判断是否是自己的子进程
		if (proc->parentId != pid) {
			return -1;
		}

		// 进程要么已结束（但未被等待，处于ZOMBIE状态），要么可运行，要么在睡眠
		assert(proc->state == ZOMBIE || procCanRun(proc) || proc->state == SLEEPING);

		// 3. 如果进程已终止(Zombie)，就回收其进程控制块
		if (proc->state == ZOMBIE) {
			procFree(proc);

			// 返回status数据
			status.bits.high8 = pid;
			copyOut(pStatus, &status, sizeof(int));
			return pid;
		} else {
			// 等待子进程结束
			__waitChild(proc, pid, pStatus, options);
		}
	} else {
		// 1. 查找是否有可等待的子进程
		if (LIST_EMPTY(&proc->childList)) {
			return -1;
		}

		struct Proc *tmp;
		LIST_FOREACH (tmp, &proc->childList, procChildLink) {
			if (tmp->state == ZOMBIE) {
				u64 childPid = proc->pid;
				procFree(proc);

				// 返回status数据
				status.bits.high8 = childPid;
				copyOut(pStatus, &status, sizeof(int));
				return childPid;
			}
		}

		// 2. 子进程尚在运行中，等待
		__waitChild(proc, pid, pStatus, options);
	}
	return -1;
}

/**
 * @brief 进程结束时，尝试唤醒父进程。如果能成功唤醒，就完成父进程未完成的wait系统调用
 */
void tryWakeupParentProc(struct Proc *child) {
	// 1. 验证父进程是否处于 "wait" 状态
	struct Proc *parent = pidToProcess(child->parentId);
	assert(parent != NULL);

	if (parent->state != SLEEPING || strncmp(parent->sleepReason, "wait", 16) != 0) {
		return;
	}

	// 2. 检查父进程的等待对象
	if (parent->wait.pid != -1) {
		if (parent->wait.pid == child->pid) {
			naiveWakeup(parent);
		} else {
			return;
		}
	} else {
		// 任意等待，必然成功
	}

	union WaitStatus status;
	status.val = 0;

	// 返回status数据
	status.bits.high8 = child->wait.exitCode;
	copyOut(parent->wait.uPtr_status, &status, sizeof(int));
}
