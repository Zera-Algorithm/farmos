#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/transfer.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#define myProc() (cpu_this()->cpu_running)

int openat(int fd, u64 filename, int flags, mode_t mode) {
	struct Dirent *dirent = NULL, *fileDirent = NULL;
	char nameBuf[NAME_MAX_LEN] = {0};
	int i, r;
	int kernFd, userFd = -1;

	copyInStr(filename, nameBuf, NAME_MAX_LEN);
	if (fd == -100) {
		dirent = myProc()->cwd;
	} else {
		// 判断相对路径还是绝对路径
		if (nameBuf[0] != '/') {
			if (fd < 0 || fd >= MAX_FD_COUNT) {
				warn("openat param fd is wrong, please check\n");
				return -1;
			} else {
				if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
					warn("kern fd is wrong, please check\n");
					return -1;
				} else {
					dirent = fds[myProc()->fdList[fd]].dirent;
				}
			}
		}
		/* else {
		    绝对路径，则不需要对dirent进行任何操作
			} */
	}

	// 检查用户可分配的Fd已分配完全
	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->fdList[i] == -1) {
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

	myProc()->fdList[userFd] = kernFd;

	return userFd;
}
