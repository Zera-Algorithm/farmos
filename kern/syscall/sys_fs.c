#include <fs/fd.h>
#include <fs/file.h>
#include <fs/kload.h>
#include <fs/pipe.h>
#include <fs/thread_fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/thread.h>
#include <sys/errno.h>
#include <sys/syscall.h>
#include <sys/syscall_fs.h>
#include <sys/time.h>

int sys_write(int fd, u64 buf, size_t count) {
	return write(fd, buf, count);
}

int sys_read(int fd, u64 buf, size_t count) {
	return read(fd, buf, count);
}

int sys_openat(int fd, u64 filename, int flags, mode_t mode) {
	return openat(fd, filename, flags, mode);
}

int sys_close(int fd) {
	return closeFd(fd);
}

int sys_dup(int fd) {
	return dup(fd);
}

int sys_dup3(int fd_old, int fd_new) {
	return dup3(fd_old, fd_new);
}

int sys_getcwd(u64 buf, int size) {
	void *kptr = cur_proc_fs_struct()->cwd;
	copyOut(buf, kptr, MIN(256, size));
	return buf;
}

int sys_pipe2(u64 pfd) {
	int fd[2];
	int ret = pipe(fd);
	if (ret < 0) {
		return ret;
	} else {
		copyOut(pfd, fd, sizeof(fd));
		return ret;
	}
}

int sys_chdir(u64 path) {
	char kbuf[MAX_NAME_LEN];
	copyInStr(path, kbuf, MAX_NAME_LEN);
	thread_fs_t *thread_fs = cur_proc_fs_struct();

	if (kbuf[0] == '/') {
		// 绝对路径
		strncpy(thread_fs->cwd, kbuf, MAX_NAME_LEN);
		assert(strlen(thread_fs->cwd) + 3 < MAX_NAME_LEN);
		strcat(thread_fs->cwd, "/"); // 保证cwd是一个目录
	} else {
		// 相对路径
		// 保证操作之前cwd以"/"结尾
		assert(strlen(thread_fs->cwd) + strlen(kbuf) + 3 < MAX_NAME_LEN);
		strcat(thread_fs->cwd, kbuf);
		strcat(thread_fs->cwd, "/");
	}
	return 0;
}

int sys_mkdirat(int dirFd, u64 path, int mode) {
	return makeDirAtFd(dirFd, path, mode);
}

int sys_mount(u64 special, u64 dir, u64 fstype, u64 flags, u64 data) {
	char specialStr[MAX_NAME_LEN];
	char dirPath[MAX_NAME_LEN];

	// 1. 将special和dir加载到字符串数组中
	copyInStr(special, specialStr, MAX_NAME_LEN);
	copyInStr(dir, dirPath, MAX_NAME_LEN);

	// 2. 计算cwd，如果dir不是绝对路径，则是相对于cwd
	Dirent *cwd = get_cwd_dirent(cur_proc_fs_struct());

	// 3. 挂载
	return mount_fs(specialStr, cwd, dirPath);
}

int sys_umount(u64 special, u64 flags) {
	char specialStr[MAX_NAME_LEN];

	// 1. 将special和dir加载到字符串数组中
	copyInStr(special, specialStr, MAX_NAME_LEN);

	// 2. 计算cwd，如果dir不是绝对路径，则是相对于cwd
	Dirent *cwd = get_cwd_dirent(cur_proc_fs_struct());

	// 3. 解除挂载
	return umount_fs(specialStr, cwd);
}

int sys_linkat(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags) {
	return linkAtFd(oldFd, pOldPath, newFd, pNewPath, flags);
}

int sys_unlinkat(int dirFd, u64 pPath) {
	return unLinkAtFd(dirFd, pPath);
}

int sys_getdents64(int fd, u64 buf, int len) {
	return getdents64(fd, buf, len);
}

int sys_fstat(int fd, u64 pkstat) {
	return fileStatFd(fd, pkstat);
}

#define TIOCGWINSZ 0x5413
struct WinSize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
} winSize = {24, 80, 0, 0};

// 调整io，一般用来获取窗口尺寸
int sys_ioctl(int fd, u64 request, u64 data) {
	if (request == TIOCGWINSZ) {
		copyOut(data, &winSize, sizeof(winSize));
	}
	// 否则不做任何操作
	return 0;
}

size_t sys_readv(int fd, const struct iovec *iov, int iovcnt) {
	return readv(fd, iov, iovcnt);
}

size_t sys_writev(int fd, const struct iovec *iov, int iovcnt) {
	return writev(fd, iov, iovcnt);
}

int sys_fstatat(int dirFd, u64 pPath, u64 pkstat, int flags) {
	return fileStatAtFd(dirFd, pPath, pkstat, flags);
}

// 因为所有write默认都会写回磁盘，所以此处可以什么也不做
int sys_fsync(int fd) {
	return 0;
}

off_t sys_lseek(int fd, off_t offset, int whence) {
	return lseekFd(fd, offset, whence);
}

int sys_faccessat(int dirFd, u64 pPath, int mode, int flags) {
	return faccessatFd(dirFd, pPath, mode, flags);
}

/**
 * @brief 原型：int ppoll(struct pollfd *fds, nfds_t nfds,
	       const struct timespec *tmo_p, const sigset_t *sigmask);
 * 等待其中一个文件描述符就绪
 * 目前的处理策略：标记所有有效fd为就绪，无视timeout和sigmask，立即返回(TODO)
 * @return fds数组中revents非0的个数。其中fds数组中，
 * revents返回events标定事件的子集或者三种特殊值：POLLERR, POLLHUP, POLLNVAL
 */
int sys_ppoll(u64 p_fds, int nfds, u64 tmo_p, u64 sigmask) {
	struct pollfd poll_fd;
	struct timespec tmo;
	int ret = 0;
	if (tmo_p)
		copyIn(tmo_p, &tmo, sizeof(tmo));

	for (int i = 0; i < nfds; i++) {
		u64 cur_fds = p_fds + i * sizeof(poll_fd);
		copyIn(cur_fds, &poll_fd, sizeof(poll_fd));
		if (poll_fd.fd < 0) {
			poll_fd.revents = 0;
		} else {
			// 目前只处理POLLIN和POLLOUT两种等待事件
			if (poll_fd.events & POLLIN) {
				poll_fd.revents |= POLLIN;
			}
			if (poll_fd.events & POLLOUT) {
				poll_fd.revents |= POLLOUT;
			}
		}
		copyOut(cur_fds, &poll_fd, sizeof(poll_fd));

		if (poll_fd.revents != 0)
			ret += 1;
	}
	return ret;
}

/**
 * @brief 控制文件描述符的属性
 */
int sys_fcntl(int fd, int cmd, int arg) {
	log(LEVEL_GLOBAL, "fcntl: fd = %d, cmd = %x, arg = %x\n", fd, cmd, arg);

	Fd *kfd;
	int ret = 0;
	if ((kfd = get_kfd_by_fd(fd)) == NULL) {
		return -1;
	}

	mtx_lock_sleep(&kfd->lock);

	switch (cmd) {
	// 目前Linux只规定了FD_CLOEXEC用于fcntl的set/get fd
	// TODO: 在exec时实现根据fd的FD_CLOEXEC flag来决定是关闭还是保留
	case FCNTL_GETFD:
		ret = kfd->flags & FD_CLOEXEC;
		break;
	case FCNTL_SETFD:
		kfd->flags |= (arg & FD_CLOEXEC);
		break;
	case FCNTL_GET_FILE_STATUS: // 等同于F_GETFL
		ret = kfd->flags;
		break;
	case FCNTL_DUPFD_CLOEXEC:
		// TODO: 实现CLOEXEC标志位
		ret = dup(fd);
		break;
	case FCNTL_SETFL:
		// TODO: 未实现
		warn("fcntl: FCNTL_SETFL not implemented\n");
		break;
	default:
		warn("fcntl: unknown cmd %d\n", cmd);
		break;
	}

	mtx_unlock_sleep(&kfd->lock);
	return ret;
}

/**
 * @brief 设置对文件的上次访问和修改时间戳
 * 针对path指定的文件，在times[0]中放置新的上次访问时间，在times[1]中放置新的上次修改时间
 * @param pTime 指向用户态const struct timespec times[2]数组的指针
 * @return 如果文件不存在，返回-1，否则返回0
 */
int sys_utimensat(int dirfd, u64 pathname, u64 pTime, int flags) {
	Dirent *dir, *file;
	char path[MAX_NAME_LEN];
	unwrap(getDirentByFd(dirfd, &dir, NULL));
	copyInStr(pathname, path, MAX_NAME_LEN);

	file = getFile(dir, path);
	if (file == NULL)
		return -ENOENT;
	else {
		file_close(file);
		return 0;
	}
}

int sys_renameat2(int olddirfd, u64 oldpath, int newdirfd, u64 newpath, unsigned int flags) {
	Dirent *olddir, *newdir;
	char oldpathStr[MAX_NAME_LEN], newpathStr[MAX_NAME_LEN];
	unwrap(getDirentByFd(olddirfd, &olddir, NULL));
	unwrap(getDirentByFd(newdirfd, &newdir, NULL));
	copyInStr(oldpath, oldpathStr, MAX_NAME_LEN);
	copyInStr(newpath, newpathStr, MAX_NAME_LEN);

	return renameat2(olddir, oldpathStr, newdir, newpathStr, flags);
}
