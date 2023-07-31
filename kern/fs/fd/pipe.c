#include <fs/fd.h>
#include <fs/fd_device.h>
#include <fs/pipe.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/proc.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <sys/errno.h>
#include <mm/kmalloc.h>

#define proc_fs_struct (cpu_this()->cpu_running->td_proc->p_fs_struct)

static int fd_pipe_read(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_pipe_write(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_pipe_close(struct Fd *fd);
static int fd_pipe_stat(struct Fd *fd, u64 pkStat);
static int pipeIsClose(struct Pipe *p);

// 定义pipe设备访问函数
// 内部的函数均应该是可重入函数，即没有static变量，不依赖全局变量（锁除外）
struct FdDev fd_dev_pipe = {
    .dev_id = 'p',
    .dev_name = "pipe",
    .dev_read = fd_pipe_read,
    .dev_write = fd_pipe_write,
    .dev_close = fd_pipe_close,
    .dev_stat = fd_pipe_stat,
};

static inline void free_both_ufd(int ufd1, int ufd2) {
	if (ufd1 >= 0)
		free_ufd(ufd1);
	if (ufd2 >= 0)
		free_ufd(ufd2);
}

extern mutex_t mtx_fd;

int pipe(int fd[2]) {
	int fd1 = -1, fd2 = -1;
	int kernfd1 = -1, kernfd2 = -1;
	u64 pipeAlloc;

	fd1 = alloc_ufd();
	fd2 = alloc_ufd();
	if (fd1 < 0 || fd2 < 0) {
		warn("no free fd in proc fdList\n");
		free_both_ufd(fd1, fd2);
		return -EMFILE;
	} else {
		if ((kernfd1 = fdAlloc()) < 0) {
			free_both_ufd(fd1, fd2);
			return kernfd1;
		}
		if ((kernfd2 = fdAlloc()) < 0) {
			free_both_ufd(fd1, fd2);
			freeFd(kernfd1);
			return kernfd2;
		}

		pipeAlloc = (u64)kmalloc(sizeof(struct Pipe));
		struct Pipe *p = (struct Pipe *)pipeAlloc;
		p->count = 2;
		p->pipeReadPos = 0;
		p->pipeWritePos = 0;
		// 初始化管道的锁
		mtx_init(&p->lock, "pipe", 1, MTX_SPIN);
		memset(p->pipeBuf, 0, PIPE_BUF_SIZE);

		fds[kernfd1].dirent = NULL;
		fds[kernfd1].pipe = (struct Pipe *)pipeAlloc;
		fds[kernfd1].type = dev_pipe;
		fds[kernfd1].flags = O_RDONLY;
		fds[kernfd1].offset = 0;
		fds[kernfd1].fd_dev = &fd_dev_pipe;
		cur_proc_fs_struct()->fdList[fd1] = kernfd1;

		fds[kernfd2].dirent = NULL;
		fds[kernfd2].pipe = (struct Pipe *)pipeAlloc;
		fds[kernfd2].type = dev_pipe;
		fds[kernfd2].flags = O_WRONLY;
		fds[kernfd2].offset = 0;
		fds[kernfd2].fd_dev = &fd_dev_pipe;
		cur_proc_fs_struct()->fdList[fd2] = kernfd2;

		fd[0] = fd1;
		fd[1] = fd2;
		return 0;
	}
}

/**
 * @brief 从管道中读取字符，允许读取少于n个字符。如果未读到字符且管道未关闭，则等待。
 * @note 传入的fd需要带锁（睡眠锁）
 * @param offset 无用参数
 */
static int fd_pipe_read(struct Fd *fd, u64 buf, u64 n, u64 offset) {

	int i;
	char ch;
	struct Pipe *p = fd->pipe;

	mtx_lock(&p->lock);
	// 如果管道为空，则一直等待
	warn("Thread %s: fd_pipe_read pipe %lx, content: %d B\n", cpu_this()->cpu_running->td_name, p, p->pipeWritePos - p->pipeReadPos);
	while (p->pipeReadPos == p->pipeWritePos && !pipeIsClose(p)) {
		// TODO：判断进程是否被kill，如被kill，就释放锁并返回负数
		// read时的channel是readPos，对方也应该以此方式唤醒
		// 睡眠时暂时放掉管道的锁

		/**
		 * 睡眠时也需要暂时放掉fd的锁，因为可能有子进程需要先关闭对端fd，再向管道写入
		 * 如果不放掉fd的锁，可能会造成死锁
		 * 此处不涉及丢失唤醒的问题。因为唤醒的主体是读写端共享的pipe锁
		 */

		mtx_unlock_sleep(&fd->lock);
		sleep(&p->pipeReadPos, &p->lock, "wait for pipe writer to write");
		mtx_lock_sleep(&fd->lock);
	}

	for (i = 0; i < n; i++) {
		if (p->pipeReadPos == p->pipeWritePos) {
			break;
		}
		ch = p->pipeBuf[p->pipeReadPos % PIPE_BUF_SIZE];
		copyOut((buf + i), &ch, 1);
		p->pipeReadPos++;
	}

	fd->offset += i;

	// 唤醒可能在等待的写者
	wakeup(&p->pipeWritePos);
	mtx_unlock(&p->lock);
	if (i == 0) {
		warn("read fd %d empty: maybe target pipe closed.\n", fd - fds);
	}
	return i;
}

static int fd_pipe_write(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	int i = 0;
	char ch;
	struct Pipe *p = fd->pipe;

	mtx_lock(&p->lock);
	warn("Thread %s: fd_pipe_write pipe %lx, content: %d B\n", cpu_this()->cpu_running->td_name, p, p->pipeWritePos - p->pipeReadPos);
	while (i < n) {
		if (pipeIsClose(p) /* || TODO: 进程已结束*/) {
			mtx_unlock(&p->lock);
			warn("writer can\'t write! pipe is closed or process is destoried.\n");
			return -EPIPE;
		}

		if (p->pipeWritePos - p->pipeReadPos == PIPE_BUF_SIZE) {
			wakeup(&p->pipeReadPos);

			mtx_unlock_sleep(&fd->lock);
			sleep(&p->pipeWritePos, &p->lock, "pipe writer wait for pipe reader.\n");
			mtx_lock_sleep(&fd->lock);
			// 唤醒之后进入下一个while轮次，继续判断管道是否关闭和进程是否结束
			// 我们采取的唤醒策略是：尽可能地接受唤醒信号，但唤醒信号不一定对本睡眠进程有效，唤醒后还需要做额外检查，若不满足条件(管道非空)应当继续睡眠
		} else {
			copyIn((buf + i), &ch, 1);
			p->pipeBuf[p->pipeWritePos % PIPE_BUF_SIZE] = ch;
			p->pipeWritePos++;
			i++;
		}
	}
	fd->offset += i;

	// 唤醒读者
	wakeup(&p->pipeReadPos);
	mtx_unlock(&p->lock);
	return i;
}

static int fd_pipe_close(struct Fd *fd) {
	struct Pipe *p = fd->pipe;
	mtx_lock(&p->lock);
	warn("Thread %s: fd_pipe_close pipe %lx, content: %d B\n", cpu_this()->cpu_running->td_name, p, p->pipeWritePos - p->pipeReadPos);
	p->count -= 1;

	// 唤醒读写端的程序。这里不需要考虑当前是读端还是写端，直接全部唤醒就可
	wakeup(&p->pipeReadPos);
	wakeup(&p->pipeWritePos);
	mtx_unlock(&p->lock);

	if (p && p->count == 0) {
		// 这里每个pipe占据一个页的空间？
		kfree(p); // 释放pipe结构体所在的物理内存
	}
	return 0;
}

// TODO: 待实现
static int fd_pipe_stat(struct Fd *fd, u64 pkStat) {
	return 0;
}

/**
 * @brief 检查管道是否关闭，必须带锁调用
 */
static int pipeIsClose(struct Pipe *p) {
	if (p != NULL) {
		if (p->count <= 1) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

/**
 * 如果管道中有数据，直接返回1
 * 如果管道中没有数据，且管道未关闭，返回0，否则返回1
 */
int pipe_check_read(struct Pipe *p) {
	mtx_lock(&p->lock);
	if (p->pipeReadPos != p->pipeWritePos) {
		mtx_unlock(&p->lock);
		return 1;
	} else if (pipeIsClose(p)) {
		mtx_unlock(&p->lock);
		return 1;
	} else {
		mtx_unlock(&p->lock);
		return 0;
	}
}

/**
 * 如果管道空闲，直接返回1
 * 如果管道不空闲，且管道未关闭，返回0，否则返回1
 */
int pipe_check_write(struct Pipe *p) {
	mtx_lock(&p->lock);
	if (p->pipeWritePos - p->pipeReadPos < PIPE_BUF_SIZE) {
		mtx_unlock(&p->lock);
		return 1;
	} else if (pipeIsClose(p)) {
		mtx_unlock(&p->lock);
		return 1;
	} else {
		mtx_unlock(&p->lock);
		return 0;
	}
}

