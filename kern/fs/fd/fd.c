#include <dev/sbi.h>
#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/fd_device.h>
#include <fs/pipe.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <lock/sleeplock.h>
#include <proc/sleep.h>

// DEPRECATED
#include <proc/cpu.h>
#define myProc() (cpu_this()->cpu_running)
// END DEPRECATED

struct mutex mtx_fd;

// 下面的读写由mtx_fd保护
static uint fdBitMap[FDNUM / 32] = {0};
struct Fd fds[FDNUM];

int getDirentByFd(int fd, Dirent **dirent, int *kernFd);

/**
 * 加锁顺序：mtx_fd, fd->lock(sleeplock)
 */

// TODO: 实现初始化
void fd_init() {
	mtx_init(&mtx_fd, "sys_fdtable", 1);

	for (int i = 0; i < FDNUM; i++) {
		// TODO!: 初始化sleeplock
	}
}

void freeFd(uint i);

/**
 * @brief 分配一个文件描述符
 * @note 此为全局操作，需要获取fd锁
 */
int fdAlloc() {
	mtx_lock(&mtx_fd);

	uint i;
	for (i = 0; i < FDNUM; i++) {
		int index = i >> 5;
		int inner = i & 31;
		if ((fdBitMap[index] & (1 << inner)) == 0) {
			fdBitMap[index] |= 1 << inner;
			fds[i].refcnt = 1;

			mtx_unlock(&mtx_fd);
			return i;
		}
	}

	mtx_unlock(&mtx_fd);
	return -1;
}

/**
 * @brief 为某个kernFd添加一个新的进程引用
 * @param i 内核fd编号
 */
void cloneAddCite(uint i) {
	assert(i >= 0 && i < FDNUM);
	fds[i].refcnt += 1; // 0 <= i < 1024
}

/**
 * @brief 释放某个进程的文件描述符
 */
int closeFd(int fd) {
	int kernFd;
	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("close param fd is wrong, please check\n");
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd is wrong, please check\n");
			return -1;
		} else {
			kernFd = myProc()->fdList[fd];
			freeFd(kernFd);
			myProc()->fdList[fd] = -1;
			return 0;
		}
	}
}

/**
 * @brief 将内核fd引用计数减一，如果引用计数归零，则回收
 */
void freeFd(uint i) {
	assert(i >= 0 && i < FDNUM);
	Fd *fd = &fds[i];

	mtx_lock(&mtx_fd);

	fd->refcnt -= 1;
	if (fd->refcnt == 0) {
		// Note 如果是file,不需要回收Dirent
		// Note 如果是pipe对应的fd关闭，则需要回收struct pipe对应的内存
		int index = i >> 5;
		int inner = i & 31;
		fdBitMap[index] &= ~(1 << inner);

		// 关闭fd对应的设备
		fd->fd_dev->dev_close(fd);

		// 释放fd的资源
		fds[i].dirent = NULL;
		fds[i].pipe = NULL;
		fds[i].type = 0;
		fds[i].offset = 0;
		fds[i].flags = 0;
		memset(&fds[i].stat, 0, sizeof(struct kstat));
	}

	mtx_unlock(&mtx_fd);
}

/**
 * @brief 读取文件描述符，返回读取的字节数。允许只读一小部分
 */
int read(int fd, u64 buf, size_t count) {
	int kernFd;
	Fd *pfd;

	unwrap(getDirentByFd(fd, NULL, &kernFd));

	pfd = &fds[kernFd];

	// 判断是否能读取
	if ((pfd->flags & O_ACCMODE) == O_WRONLY) {
		warn("fd can not be read\n");

		return -1;
	}

	// 处理dev_read
	int ret = pfd->fd_dev->dev_read(pfd, buf, count, pfd->offset);
	return ret;
}

int write(int fd, u64 buf, size_t count) {
	int kernFd;
	Fd *pfd;

	unwrap(getDirentByFd(fd, NULL, &kernFd));

	pfd = &fds[kernFd];

	// 判断是否能写入
	if ((pfd->flags & O_ACCMODE) == O_RDONLY) {
		warn("fd can not be write\n");

		return -1;
	}

	// 处理dev_write
	int ret = pfd->fd_dev->dev_read(pfd, buf, count, pfd->offset);
	return ret;
}

int dup(int fd) {
	int newFd = -1;
	int kernFd;
	int i;

	unwrap(getDirentByFd(fd, NULL, &kernFd));
	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->fdList[i] == -1) {
			newFd = i;
			break;
		}
	}
	if (newFd < 0) {
		warn("no free fd in proc fdList\n");
		return -1;
	}

	myProc()->fdList[newFd] = kernFd;
	cloneAddCite(kernFd);
	return newFd;
}

int dup3(int old, int new) {
	int copied;

	if (old < 0 || old >= MAX_FD_COUNT) {
		warn("dup param old is wrong, please check\n");
		return -1;
	}
	if (new < 0 || new >= MAX_FD_COUNT) {
		warn("dup param new[] is wrong, please check\n");
		return -1;
	}
	if (myProc()->fdList[new] >= 0 && myProc()->fdList[new] < FDNUM) {
		freeFd(myProc()->fdList[new]);
	} else if (myProc()->fdList[new] >= FDNUM) {
		warn("kern fd is wrong, please check\n");
		return -1;
	}
	if (myProc()->fdList[old] < 0 || myProc()->fdList[old] >= FDNUM) {
		warn("kern fd is wrong, please check\n");
		return -1;
	}
	copied = myProc()->fdList[old];
	myProc()->fdList[new] = copied;

	// kernFd引用计数加1
	cloneAddCite(copied);
	return new;
}

/**
 * @brief 检索fd对应的Dirent和kernFd，同时处理错误
 */
int getDirentByFd(int fd, Dirent **dirent, int *kernFd) {
	if (fd == AT_FDCWD) {
		if (dirent) {
			*dirent = myProc()->cwd;
			return 0;
		}
		// dirent无效时，由于AT_FDCWD是负数，应当继续下面的流程，直到报错
	}

	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("write param fd(%d) is wrong, please check\n", fd);
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd(%d) is wrong, please check\n", myProc()->fdList[fd]);
			return -1;
		} else {
			int kFd = myProc()->fdList[fd];
			if (kernFd)
				*kernFd = kFd;
			if (dirent)
				*dirent = fds[kFd].dirent;
			return 0;
		}
	}
}

// 以下不涉及设备的读写访问

int getdents64(int fd, u64 buf, int len) {
	Dirent *dir, *file;
	int kernFd, ret, offset;
	unwrap(getDirentByFd(fd, &dir, &kernFd));

	DirentUser direntUser;
	direntUser.d_ino = 0;
	direntUser.d_reclen = DIRENT_USER_SIZE;
	direntUser.d_type = dev_file;
	ret = dirGetDentFrom(dir, fds[kernFd].offset, &file, &offset, NULL);
	direntUser.d_off = offset;
	fds[kernFd].offset = offset;

	strncpy(direntUser.d_name, file->name, DIRENT_NAME_LENGTH);

	if (ret == 0) {
		warn("read dirents to the end! dir: %s\n", dir->name);
	} else {
		ret = DIRENT_USER_SIZE;
	}
	copyOut(buf, &direntUser, DIRENT_USER_SIZE);

	return ret;
}

int makeDirAtFd(int dirFd, u64 path, int mode) {
	Dirent *dir;
	char name[MAX_NAME_LEN];

	unwrap(getDirentByFd(dirFd, &dir, NULL));
	copyInStr(path, name, MAX_NAME_LEN);

	log(LEVEL_GLOBAL, "make dir %s at %s\n", name, dir->name);
	return makeDirAt(dir, name, mode);
}

int linkAtFd(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags) {
	struct Dirent *oldDir, *newDir;
	char oldPath[MAX_NAME_LEN];
	char newPath[MAX_NAME_LEN];
	unwrap(getDirentByFd(oldFd, &oldDir, NULL));
	unwrap(getDirentByFd(newFd, &newDir, NULL));
	copyInStr(pOldPath, oldPath, MAX_NAME_LEN);
	copyInStr(pNewPath, newPath, MAX_NAME_LEN);
	return linkat(oldDir, oldPath, newDir, newPath);
}

int unLinkAtFd(int dirFd, u64 pPath) {
	struct Dirent *dir;
	char path[MAX_NAME_LEN];
	unwrap(getDirentByFd(dirFd, &dir, NULL));
	copyInStr(pPath, path, MAX_NAME_LEN);
	return unlinkat(dir, path);
}

int fileStatFd(int fd, u64 pkstat) {
	int kFd;
	unwrap(getDirentByFd(fd, NULL, &kFd));
	Fd *kernFd = &fds[kFd];
	unwrap(kernFd->fd_dev->dev_stat(kernFd, pkstat));
	return 0;
}
