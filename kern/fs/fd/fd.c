#include <dev/sbi.h>
#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/fd_device.h>
#include <fs/file.h>
#include <fs/pipe.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <mm/kmalloc.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <sys/errno.h>
#include <sys/syscall_fs.h>

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
	mtx_init(&mtx_fd, "sys_fdtable", 1, MTX_SPIN | MTX_RECURSE);
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
			mtx_init(&fds[i].lock, "fd_lock", 1, MTX_SLEEP);

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
	mtx_lock(&fds[i].lock);

	assert(i >= 0 && i < FDNUM);
	fds[i].refcnt += 1; // 0 <= i < 1024

	mtx_unlock(&fds[i].lock);
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
		if (cur_proc_fs_struct()->fdList[fd] < 0 ||
		    cur_proc_fs_struct()->fdList[fd] >= FDNUM) {
			warn("kern fd is wrong, please check\n");
			return -1;
		} else {
			kernFd = cur_proc_fs_struct()->fdList[fd];
			freeFd(kernFd);
			cur_proc_fs_struct()->fdList[fd] = -1;
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

	mtx_lock_sleep(&fd->lock);
	fd->refcnt -= 1;
	if (fd->refcnt == 0) {
		// Note 如果是file,不需要回收Dirent
		// Note 如果是pipe对应的fd关闭，则需要回收struct pipe对应的内存

		mtx_lock(&mtx_fd);
		int index = i >> 5;
		int inner = i & 31;
		fdBitMap[index] &= ~(1 << inner);
		mtx_unlock(&mtx_fd);

		// 关闭fd对应的设备
		if (fd->fd_dev != NULL) { // 可能是新创建的kernFd，因为找不到文件而失败，被迫释放
			fd->fd_dev->dev_close(fd);
		}

		// 释放fd的资源
		fds[i].dirent = NULL;
		fds[i].pipe = NULL;
		fds[i].socket = NULL;
		fds[i].type = 0;
		fds[i].offset = 0;
		fds[i].flags = 0;

		mtx_unlock_sleep(&fd->lock); // 此时fd已不可能被查询到，故可以安心放锁
		memset(&fds[i].stat, 0, sizeof(struct kstat));
	} else {
		mtx_unlock_sleep(&fd->lock);
	}
}

/**
 * @brief 读取文件描述符，返回读取的字节数。允许只读一小部分
 */
int read(int fd, u64 buf, size_t count) {
	int kernFd;
	Fd *pfd;

	unwrap(getDirentByFd(fd, NULL, &kernFd));
	pfd = &fds[kernFd];
	mtx_lock_sleep(&pfd->lock);

	// 判断是否能读取
	if ((pfd->flags & O_ACCMODE) == O_WRONLY) {
		warn("fd %d can not be read\n", fd);

		mtx_unlock_sleep(&pfd->lock);
		return -1;
	}

	// 处理dev_read
	int ret = pfd->fd_dev->dev_read(pfd, buf, count, pfd->offset);

	mtx_unlock_sleep(&pfd->lock);
	return ret;
}

// readv是一个原子操作，单次调用readv读取内容不会被其他进程打断
// readv如果中间实际读取的长度小于需要读的长度，则可以中途返回
size_t readv(int fd, const struct iovec *iov, int iovcnt) {
	int kernFd;
	Fd *pfd;
	int len = 0, total = 0; // total表示总计读取的字节数
	struct iovec iov_temp;

	unwrap(getDirentByFd(fd, NULL, &kernFd));
	pfd = &fds[kernFd];
	mtx_lock_sleep(&pfd->lock);

	// 判断是否能读取
	if ((pfd->flags & O_ACCMODE) == O_WRONLY) {
		warn("fd can not be read\n");

		mtx_unlock_sleep(&pfd->lock);
		return -1;
	}

	// 处理dev_read
	for (int i = 0; i < iovcnt; i++) {
		// iov数组在用户态，需要copyIn读入
		copy_in(cur_proc_pt(), (u64)(&iov[i]), &iov_temp, sizeof(struct iovec));
		len = pfd->fd_dev->dev_read(pfd, (u64)iov_temp.iov_base, iov_temp.iov_len,
					    pfd->offset);
		if (len < 0) {
			// 读取出现问题，直接返回错误值
			mtx_unlock_sleep(&pfd->lock);
			return len;
		}
		total += len;
		pfd->offset += len;

		if (len < iov_temp.iov_len) {
			// 读取结束，读不到更多数据，直接返回
			mtx_unlock_sleep(&pfd->lock);
			return total;
		}
	}

	mtx_unlock_sleep(&pfd->lock);
	return total;
}

int write(int fd, u64 buf, size_t count) {
	int kernFd;
	Fd *pfd;

	unwrap(getDirentByFd(fd, NULL, &kernFd));

	pfd = &fds[kernFd];

	mtx_lock_sleep(&pfd->lock);

	// 判断是否能写入
	if ((pfd->flags & O_ACCMODE) == O_RDONLY) {
		warn("fd can not be write\n");

		mtx_unlock_sleep(&pfd->lock);
		return -1;
	}

	// 处理dev_write
	int ret = pfd->fd_dev->dev_write(pfd, buf, count, pfd->offset);
	mtx_unlock_sleep(&pfd->lock);
	return ret;
}

// writev是一个原子操作，单次调用writev读取内容不会被其他进程打断
// writev尽力写入所有数据
size_t writev(int fd, const struct iovec *iov, int iovcnt) {
	int kernFd;
	Fd *pfd;
	int len = 0, total = 0; // total表示总计读取的字节数
	struct iovec iov_temp;

	unwrap(getDirentByFd(fd, NULL, &kernFd));
	pfd = &fds[kernFd];
	mtx_lock_sleep(&pfd->lock);

	// 判断是否能写入
	if ((pfd->flags & O_ACCMODE) == O_RDONLY) {
		warn("fd can not be read\n");
		mtx_unlock_sleep(&pfd->lock);
		return -1;
	}

	// 处理dev_write
	for (int i = 0; i < iovcnt; i++) {
		copy_in(cur_proc_pt(), (u64)(&iov[i]), &iov_temp, sizeof(struct iovec));
		len = pfd->fd_dev->dev_write(pfd, (u64)iov_temp.iov_base, iov_temp.iov_len,
					     pfd->offset);
		if (len < 0) {
			// 写入出现问题，直接返回错误值
			mtx_unlock_sleep(&pfd->lock);
			return len;
		}
		total += len;
		pfd->offset += len;

		if (len < iov_temp.iov_len) {
			// 写入结束，写不进更多数据，直接返回
			// 一般的write会尽力写入所有给定的数据，但是如果是管道关闭，那么可能会只写入部分数据
			// 此时之后的数据也一定写不进去了，直接返回
			mtx_unlock_sleep(&pfd->lock);
			return total;
		}
	}

	mtx_unlock_sleep(&pfd->lock);
	return total;
}

int dup(int fd) {
	int newFd = -1;
	int kernFd;
	int i;

	unwrap(getDirentByFd(fd, NULL, &kernFd));
	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (cur_proc_fs_struct()->fdList[i] == -1) {
			newFd = i;
			break;
		}
	}
	if (newFd < 0) {
		warn("no free fd in proc fdList\n");
		return -1;
	}

	cur_proc_fs_struct()->fdList[newFd] = kernFd;
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
	if (cur_proc_fs_struct()->fdList[new] >= 0 && cur_proc_fs_struct()->fdList[new] < FDNUM) {
		freeFd(cur_proc_fs_struct()->fdList[new]);
	} else if (cur_proc_fs_struct()->fdList[new] >= FDNUM) {
		warn("kern fd is wrong, please check\n");
		return -1;
	}
	if (cur_proc_fs_struct()->fdList[old] < 0 || cur_proc_fs_struct()->fdList[old] >= FDNUM) {
		warn("kern fd is wrong, please check\n");
		return -1;
	}
	copied = cur_proc_fs_struct()->fdList[old];
	cur_proc_fs_struct()->fdList[new] = copied;

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
			*dirent = get_cwd_dirent(cur_proc_fs_struct());
			if (*dirent == NULL) {
				warn("cwd fd is invalid: %s\n", cur_proc_fs_struct()->cwd);
				return -EBADF; // 需要检查所有调用此函数的上级函数是否有依赖
			}
			return 0;
		}
		// dirent无效时，由于AT_FDCWD是负数，应当继续下面的流程，直到报错
	}

	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("write param fd(%d) is wrong, please check\n", fd);
		return -EBADF;
	} else {
		if (cur_proc_fs_struct()->fdList[fd] < 0 ||
		    cur_proc_fs_struct()->fdList[fd] >= FDNUM) {
			warn("kern fd(%d) is wrong, please check\n",
			     cur_proc_fs_struct()->fdList[fd]);
			return -EBADF;
		} else {
			int kFd = cur_proc_fs_struct()->fdList[fd];
			if (kernFd)
				*kernFd = kFd;
			if (dirent)
				*dirent = fds[kFd].dirent;
			return 0;
		}
	}
}

/**
 * @brief 测试fd是否有效，有效返回kfd(不带锁)，无效返回NULL
 * 不接受fd为AT_CWD的情况，如果需要请移步getDirentByFd
 */
Fd *get_kfd_by_fd(int fd) {
	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("write param fd(%d) is wrong, please check\n", fd);
		return NULL;
	} else {
		if (cur_proc_fs_struct()->fdList[fd] < 0 ||
		    cur_proc_fs_struct()->fdList[fd] >= FDNUM) {
			warn("kern fd(%d) is wrong, please check\n",
			     cur_proc_fs_struct()->fdList[fd]);
			return NULL;
		} else {
			int kfd = cur_proc_fs_struct()->fdList[fd];
			return &fds[kfd];
		}
	}
}

// 以下不涉及设备的读写访问

int getdents64(int fd, u64 buf, int len) {
	Dirent *dir, *file;
	int kernFd, ret, offset;
	unwrap(getDirentByFd(fd, &dir, &kernFd));

	DirentUser *direntUser = kmalloc(DIRENT_USER_SIZE);
	direntUser->d_ino = 0;
	direntUser->d_reclen = DIRENT_USER_SIZE;
	direntUser->d_type = dev_file;
	ret = dirGetDentFrom(dir, fds[kernFd].offset, &file, &offset, NULL);
	direntUser->d_off = offset;
	fds[kernFd].offset = offset;

	if (ret == 0) {
		// 读到了目录尾部，此时file为NULL
		warn("read dirents to the end! dir: %s\n", dir->name);
		direntUser->d_name[0] = '\0';
		copyOut(buf, direntUser, DIRENT_USER_SIZE);
		kfree(direntUser);
		return 0;
	} else {
		strncpy(direntUser->d_name, file->name, DIRENT_NAME_LENGTH);
		copyOut(buf, direntUser, DIRENT_USER_SIZE);
		dirent_dealloc(file);
		kfree(direntUser);
		return DIRENT_USER_SIZE;
	}
}

/**
 * @brief makeDirAtFd 在dirFd指定的目录下创建目录。请求进程仅创建目录，不持有创建目录的引用
 */
int makeDirAtFd(int dirFd, u64 path, int mode) {
	Dirent *dir;
	int ret;
	char name[MAX_NAME_LEN];

	unwrap(getDirentByFd(dirFd, &dir, NULL));
	copyInStr(path, name, MAX_NAME_LEN);

	log(LEVEL_GLOBAL, "make dir %s at %s\n", name, dir->name);
	ret = makeDirAt(dir, name, mode);
	return ret;
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

// 以下两个stat的最终实现位于fat32/file.c的fileStat函数

/**
 * @brief 以fd为媒介获取文件状态
 */
int fileStatFd(int fd, u64 pkstat) {
	int kFd;
	unwrap(getDirentByFd(fd, NULL, &kFd));
	Fd *kernFd = &fds[kFd];

	mtx_lock_sleep(&kernFd->lock);
	unwrap(kernFd->fd_dev->dev_stat(kernFd, pkstat));
	mtx_unlock_sleep(&kernFd->lock);
	return 0;
}

/**
 * @brief 以fd+path为凭证获取文件状态
 * @note 需要获取和关闭dirent，在fileStat中会对dirent层加锁
 * @todo 需要处理flags为AT_NO_AUTOMOUNT或AT_EMPTY_PATH的情况
 */
int fileStatAtFd(int dirFd, u64 pPath, u64 pkstat, int flags) {
	Dirent *baseDir, *file;
	char path[MAX_NAME_LEN];
	unwrap(getDirentByFd(dirFd, &baseDir, NULL));
	copyInStr(pPath, path, MAX_NAME_LEN);

	log(LEVEL_GLOBAL, "fstat %s, dirFd is %d, flags = %x\n", path, dirFd, flags);

	if (flags & AT_SYMLINK_NOFOLLOW) {
		// 不跟随符号链接
		file = get_file_raw(baseDir, path);
	} else {
		file = getFile(baseDir, path);
	}
	if (file == NULL) {
		warn("can't find file %s at fd %d\n", path, dirFd);
		return -ENOENT;
	}
	struct kstat kstat;
	fileStat(file, &kstat);
	copyOut(pkstat, &kstat, sizeof(struct kstat));

	file_close(file);
	return 0;
}

off_t lseekFd(int fd, off_t offset, int whence) {
	int kFd_num;
	struct kstat kstat;
	unwrap(getDirentByFd(fd, NULL, &kFd_num));
	Fd *kernFd = &fds[kFd_num];

	mtx_lock_sleep(&kernFd->lock);
	if (whence == SEEK_SET || whence == SEEK_DATA) {
		kernFd->offset = offset;
	} else if (whence == SEEK_CUR) {
		kernFd->offset += offset;
	} else if (whence == SEEK_END) {
		// 只有磁盘文件才能获取到文件的大小
		if (kernFd->type != dev_file) {
			warn("only file can use SEEK_END\n");
			mtx_unlock_sleep(&kernFd->lock);
			return -1;
		} else {
			fileStat(kernFd->dirent, &kstat);
			kernFd->offset = kstat.st_size + offset;
		}
	} else if (whence == SEEK_HOLE) {
		fileStat(kernFd->dirent, &kstat);
		// 文件中没有任何空洞，设置offset为文件尾部
		kernFd->offset = kstat.st_size;
	} else {
		warn("unknown lseek whence %d\n", whence);
	}
	mtx_unlock_sleep(&kernFd->lock);
	return 0;
}

int faccessatFd(int dirFd, u64 pPath, int mode, int flags) {
	Dirent *dir;
	char path[MAX_NAME_LEN];
	unwrap(getDirentByFd(dirFd, &dir, NULL));
	copyInStr(pPath, path, MAX_NAME_LEN);
	return faccessat(dir, path, mode, flags);
}
