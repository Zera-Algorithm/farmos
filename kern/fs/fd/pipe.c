#include <fs/fd.h>
#include <fs/fd_device.h>
#include <fs/pipe.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/sleep.h>
#include <proc/thread.h>

#define myProc() (cpu_this()->cpu_running)

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

extern mutex_t mtx_fd;

int pipe(int fd[2]) {
	int fd1 = -1, fd2 = -1;
	int kernfd1 = -1, kernfd2 = -1;
	int i;
	u64 pipeAlloc;

	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->td_fs_struct.fdList[i] == -1) {
			fd1 = i;
			break;
		}
	}
	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->td_fs_struct.fdList[i] == -1 && i != fd1) {
			fd2 = i;
			break;
		}
	}
	if (fd1 < 0 || fd2 < 0) {
		warn("no free fd in proc fdList\n");
		return -1;
	} else {
		kernfd1 = fdAlloc();
		if (kernfd1 < 0) {
			warn("no free fd in os\n");
			return -1;
		}
		kernfd2 = fdAlloc();
		if (kernfd2 < 0) {
			warn("no free fd in os\n");
			freeFd(kernfd1);
			return -1;
		}

		pipeAlloc = kvmAlloc();
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
		myProc()->td_fs_struct.fdList[fd1] = kernfd1;

		fds[kernfd2].dirent = NULL;
		fds[kernfd2].pipe = (struct Pipe *)pipeAlloc;
		fds[kernfd2].type = dev_pipe;
		fds[kernfd2].flags = O_WRONLY;
		fds[kernfd2].offset = 0;
		fds[kernfd2].fd_dev = &fd_dev_pipe;
		myProc()->td_fs_struct.fdList[fd2] = kernfd2;

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
	while (p->pipeReadPos == p->pipeWritePos && !pipeIsClose(p)) {
		// TODO：判断进程是否被kill，如被kill，就释放锁并返回负数
		// read时的channel是readPos，对方也应该以此方式唤醒
		// 睡眠时暂时放掉管道的锁
		sleep(&p->pipeReadPos, &p->lock, "wait for pipe writer to write");
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
	return i;
}

static int fd_pipe_write(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	int i = 0;
	char ch;
	struct Pipe *p = fd->pipe;

	mtx_lock(&p->lock);
	while (i < n) {
		if (pipeIsClose(p) /* || TODO: 进程已结束*/) {
			mtx_unlock(&p->lock);
			warn("writer can\'t write! pipe is closed or process is destoried.\n");
			return -1;
		}

		if (p->pipeWritePos - p->pipeReadPos == PIPE_BUF_SIZE) {
			wakeup(&p->pipeReadPos);
			sleep(&p->pipeWritePos, &p->lock, "pipe writer wait for pipe reader.\n");
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
	return i;
}

static int fd_pipe_close(struct Fd *fd) {
	struct Pipe *p = fd->pipe;
	p->count -= 1;

	// 唤醒读写端的程序。这里不需要考虑当前是读端还是写端，直接全部唤醒就可
	wakeup(&p->pipeReadPos);
	wakeup(&p->pipeWritePos);

	if (p && p->count == 0) {
		// 这里每个pipe占据一个页的空间？
		kvmFree((u64)p); //释放pipe结构体所在的物理内存
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
