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
#include <sys/syscall.h>
#include <sys/syscall_fs.h>

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

int sys_newfstatat(int dirFd, u64 pPath, u64 pkstat, int flags) {
	return fileStatAtFd(dirFd, pPath, pkstat, flags);
}
