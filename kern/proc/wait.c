#include <lib/log.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/sleep.h>
#include <proc/thread.h>

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
 * @brief 将传入的僵尸子进程回收，需要已持有子进程的锁。
 */
static u64 wait_exit(thread_t *curtd, thread_t *child, u64 pstatus) {
	u64 tid = child->td_tid;
	wstatus_t s = {.bits.high8 = child->td_exitcode};
	if (pstatus != 0) {
		// 将子进程的退出状态写入父进程的内存空间
		copy_out(curtd->td_pt, pstatus, &s, sizeof(u32));
	}
	// 更新父进程中记录子进程运行时间的字段
	curtd->td_times.tms_cutime += child->td_times.tms_utime;
	curtd->td_times.tms_cstime += child->td_times.tms_stime;

	td_free(child);

	LIST_REMOVE(child, td_childentry);

	mtx_unlock(&child->td_lock);
	mtx_unlock(&wait_lock);
	return tid;
}

static u64 wait_til_exit(thread_t *curtd, thread_t *child, u64 pstatus, int options) {
	if (options & WNOHANG) {
		// 若设置了 WNOHANG，直接返回
		mtx_unlock(&child->td_lock);
		mtx_unlock(&wait_lock);
		return WAIT_NOHANG_EXIT;
	}
	// 进入时已持有子进程的锁
	while (child->td_status != ZOMBIE) {
		mtx_unlock(&child->td_lock);
		sleep(curtd, &wait_lock, "waiting for child to exit");
		mtx_lock(&child->td_lock);
	}
	return wait_exit(curtd, child, pstatus);
}

u64 wait(thread_t *curtd, i64 pid, u64 pstatus, int options) {
	// 获取等待锁，且不应该有其他锁
	assert(cpu_this()->cpu_lk_depth == 0);
	mtx_lock(&wait_lock);

	// 检查是否有能够等待的子进程
	if (LIST_EMPTY(&curtd->td_childlist)) {
		mtx_unlock(&wait_lock);
		return -1;
	}

	// 有子进程，需要等待并释放一个子进程再返回
	while (1) {
		thread_t *child;
		LIST_FOREACH (child, &curtd->td_childlist, td_childentry) {
			mtx_lock(&child->td_lock);
			if (pid != -1 && pid == child->td_tid) {
				// 若找到了目标进程，等待其结束后回收
				assert(child->td_status != UNUSED);
				return wait_til_exit(curtd, child, pstatus, options);
			} else if (pid == -1 && child->td_status == ZOMBIE) {
				return wait_exit(curtd, child, pstatus);
			}
			// 余下情况为：
			// 1. pid != -1 且 pid != child->td_tid，继续检查下一个子进程
			// 2. pid == -1 且 child->td_status != ZOMBIE，继续检查下一个子进程
			mtx_unlock(&child->td_lock);
		}

		if (pid != -1) {
			// 此时如果没有找到目标进程，说明目标进程不在子进程列表中，返回
			mtx_unlock(&wait_lock);
			return -1;
		} else if (options & WNOHANG) {
			// 未指定目标进程，且设置了 WNOHANG，直接返回
			mtx_unlock(&wait_lock);
			return WAIT_NOHANG_EXIT;
		} else {
			// 未指定目标进程，且未设置 WNOHANG，等待子进程退出
			sleep(curtd, &wait_lock, "waiting for child to exit");
		}
	}
}
