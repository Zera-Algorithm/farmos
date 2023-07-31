#include <lib/log.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <sys/errno.h>

#define WAIT_FAIL -1
#define WAIT_NOHANG_EXIT 0

#define WNOHANG 1    /* Don't block waiting.  */
#define WUNTRACED 2  /* Report status of stopped children.  */
#define WCONTINUED 8 /* Report continued child.  */

typedef union wstatus {
	u32 val; // 整体值

	// 从低地址到高地址
	struct {
		unsigned low8 : 8;
		unsigned high8 : 8;
		unsigned __empty : 16;
	} __attribute__((packed)) bits; // 取消优化对齐
} wstatus_t;

/**
 * @brief 等待锁，保证父进程等待和子进程退出按顺序依次发生。
 * @note 需要保证在获取任意一个进程的锁之前，先获取 wait_lock。
 * @attention 父进程检查每个子进程时，先获取
 * wait_lock，再获取子进程的锁，因此子进程在调度时才放自己的锁。
 */
mutex_t wait_lock;

/**
 * @brief 将传入的僵尸子进程回收，需要已持有当前进程锁和子进程的锁。
 */
static u64 wait_exit(thread_t *curtd, proc_t *childp, u64 pstatus) {
	u64 pid = childp->p_pid;
	wstatus_t s = {.bits.high8 = childp->p_exitcode};
	if (pstatus != 0) {
		// 将子进程的退出状态写入父进程的内存空间
		copy_out(curtd->td_pt, pstatus, &s, sizeof(u32));
	}
	// 更新父进程中记录子进程运行时间的字段
	curtd->td_proc->p_times.tms_cutime += childp->p_times.tms_utime;
	curtd->td_proc->p_times.tms_cstime += childp->p_times.tms_stime;

	proc_free(childp);
	LIST_REMOVE(childp, p_sibling); // p_children 中删除，已持有父进程的锁

	proc_unlock(childp);
	proc_unlock(curtd->td_proc);
	mtx_unlock(&wait_lock);
	return pid;
}

static u64 wait_til_exit(thread_t *curtd, proc_t *childp, u64 pstatus, int options) {
	if (options & WNOHANG) {
		// 若设置了 WNOHANG，直接返回
		proc_unlock(childp);
		proc_unlock(curtd->td_proc);
		mtx_unlock(&wait_lock);
		return WAIT_NOHANG_EXIT;
	}
	// 进入时已持有子进程的锁
	while (childp->p_status != ZOMBIE) {
		proc_unlock(childp);
		proc_unlock(curtd->td_proc);
		sleep(curtd->td_proc, &wait_lock, "waiting for child to exit");
		proc_lock(curtd->td_proc);
		proc_lock(childp);
	}
	return wait_exit(curtd, childp, pstatus);
}

u64 wait(thread_t *curtd, i64 pid, u64 pstatus, int options) {
	// 获取等待锁，且不应该有其他锁
	assert(cpu_this()->cpu_lk_depth == 0);
	mtx_lock(&wait_lock);

	// 检查是否有能够等待的子进程
	if (LIST_EMPTY(&curtd->td_proc->p_children)) {
		mtx_unlock(&wait_lock);
		warn("no child process\n");
		return -ECHILD;
	}

	// 有子进程，需要等待并释放一个子进程再返回
	while (1) {
		proc_t *childp;
		proc_lock(curtd->td_proc);
		LIST_FOREACH (childp, &curtd->td_proc->p_children, p_sibling) {
			proc_lock(childp);
			if (pid != -1 && pid == childp->p_pid) {
				// 若找到了目标进程，等待其结束后回收
				assert(childp->p_status != UNUSED);
				return wait_til_exit(curtd, childp, pstatus, options);
			} else if (pid == -1 && childp->p_status == ZOMBIE) {
				return wait_exit(curtd, childp, pstatus);
			}
			// 余下情况为：
			// 1. pid != -1 且 pid != child->td_tid，继续检查下一个子进程
			// 2. pid == -1 且 child->td_status != ZOMBIE，继续检查下一个子进程
			proc_unlock(childp);
		}
		proc_unlock(curtd->td_proc);

		if (pid != -1) {
			// 此时如果没有找到目标进程，说明目标进程不在子进程列表中，返回
			mtx_unlock(&wait_lock);
			warn("no such child process: pid = %lx\n", pid);
			return -ECHILD;
		} else if (options & WNOHANG) {
			// 未指定目标进程，且设置了 WNOHANG，直接返回
			mtx_unlock(&wait_lock);
			return WAIT_NOHANG_EXIT;
		} else {
			// 未指定目标进程，且未设置 WNOHANG，等待子进程退出
			sleep(curtd->td_proc, &wait_lock, "waiting for child to exit");
		}
	}
}
