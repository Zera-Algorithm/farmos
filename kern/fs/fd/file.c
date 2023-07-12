#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/fd_device.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#define myProc() (cpu_this()->cpu_running)

static int fd_file_read(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_file_write(struct Fd *fd, u64 buf, u64 n, u64 offset);
int fd_file_close(struct Fd *fd);
int fd_file_stat(struct Fd *fd, u64 pkStat);

// 定义console设备访问函数
// 内部的函数均应该是可重入函数，即没有static变量，不依赖全局变量（锁除外）
struct FdDev fd_dev_file = {
    .dev_id = 'f',
    .dev_name = "file",
    .dev_read = fd_file_read,
    .dev_write = fd_file_write,
    .dev_close = fd_file_close,
    .dev_stat = fd_file_stat,
};

// 注意：设备的相关读写均不需要对fd加锁
// Fd的锁应当由fd.c维护

int openat(int fd, u64 filename, int flags, mode_t mode) {
	struct Dirent *dirent = NULL, *fileDirent = NULL;
	char nameBuf[NAME_MAX_LEN] = {0};
	int i, r;
	int kernFd, userFd = -1;

	copyInStr(filename, nameBuf, NAME_MAX_LEN);
	if (fd == AT_FDCWD) {
		dirent = get_cwd_dirent(&(myProc()->td_fs_struct));
		assert(dirent != NULL);
	} else {
		// 判断相对路径还是绝对路径
		if (nameBuf[0] != '/') {
			if (fd < 0 || fd >= MAX_FD_COUNT) {
				warn("openat param fd is wrong, please check\n");
				return -1;
			} else {
				if (myProc()->td_fs_struct.fdList[fd] < 0 ||
				    myProc()->td_fs_struct.fdList[fd] >= FDNUM) {
					warn("kern fd is wrong, please check\n");
					return -1;
				} else {
					dirent = fds[myProc()->td_fs_struct.fdList[fd]].dirent;
				}
			}
		}
		/* else {
		    绝对路径，则不需要对dirent进行任何操作
			} */
	}

	// 检查用户可分配的Fd已分配完全
	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->td_fs_struct.fdList[i] == -1) {
			userFd = i;
			break;
		}
	}

	if (userFd < 0) {
		warn("no free fd in proc fdList\n");
		return -1;
	}

	kernFd = fdAlloc();

	if (kernFd < 0) {
		warn("no free fd in kern fds\n");
		return -1;
	}

	// fix: O_CREATE表示若文件不存在，则创建一个
	// 打开，不含创建
	fileDirent = getFile(dirent, nameBuf);
	if (fileDirent == NULL) {
		if ((flags & O_CREATE) == O_CREATE) {
			// 创建
			r = createFile(dirent, nameBuf, &fileDirent);
			if (r < 0) {
				freeFd(kernFd);
				warn("create file fail: r = %d\n", r);
				return -1;
			}
		} else {
			freeFd(kernFd);
			warn("get file fail\n");
			return -1;
		}
	}

	fds[kernFd].dirent = fileDirent;
	fds[kernFd].pipe = NULL;
	fds[kernFd].type = dev_file;
	fds[kernFd].offset = 0;
	fds[kernFd].flags = flags;
	fds[kernFd].stat.st_mode = mode;
	fds[kernFd].fd_dev = &fd_dev_file; // 设置dev

	myProc()->td_fs_struct.fdList[userFd] = kernFd;

	return userFd;
}

// 读一个文件，返回读取的字节数
static int fd_file_read(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	Dirent *dirent = fd->dirent;
	n = file_read(dirent, 1, buf, fd->offset, n);
	if (n < 0) {
		warn("file read num is below zero\n");
		return -1;
	}
	fd->offset += n;
	return n;
}

static int fd_file_write(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	Dirent *dirent = fd->dirent;
	n = file_write(dirent, 1, buf, fd->offset, n);
	if (n < 0) {
		warn("file read num is below zero\n");
		return -1;
	}
	fd->offset += n;
	return n;
}

// 文件关闭暂不需要额外动作
int fd_file_close(struct Fd *fd) {
	return 0;
}

int fd_file_stat(struct Fd *fd, u64 pkStat) {
	struct Dirent *file = fd->dirent;
	struct kstat kstat;
	fileStat(file, &kstat);
	copyOut(pkStat, &kstat, sizeof(struct kstat));
	return 0;
}
