/**
 * @brief 本文件规定与thread的通用接口，包括init, fork, recycle三个动作时对进程管理的fs结构的处理
 */

#include <fs/console.h>
#include <fs/fd.h>
#include <fs/file.h>
#include <fs/thread_fs.h>
#include <fs/vfs.h>
#include <lib/string.h>

void init_thread_fs(thread_fs_t *fs_struct) {
	strncpy(fs_struct->cwd, "/", 2);

	// 初始化文件描述符表
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		fs_struct->fdList[i] = -1;
	}

	// 初始化控制台文件描述符
	fs_struct->fdList[0] = readConsoleAlloc();
	fs_struct->fdList[1] = writeConsoleAlloc();
	fs_struct->fdList[2] = errorConsoleAlloc();

	fs_struct->cwd_dirent = NULL;

	// 初始化MMAP区域的开始位置
	fs_struct->mmap_addr = MMAP_START;
}

void fork_thread_fs(thread_fs_t *old, thread_fs_t *new) {
	strncpy(new->cwd, old->cwd, MAX_NAME_LEN);

	// 复制父进程已打开的文件
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		int kFd = old->fdList[i];
		if (kFd != -1) {
			cloneAddCite(kFd);
		}
		new->fdList[i] = kFd;
	}

	// fork时子进程继承父进程的MMAP
	new->mmap_addr = old->mmap_addr;
}

void recycle_thread_fs(thread_fs_t *fs_struct) {
	fs_struct->cwd[0] = 0;

	// 回收进程的文件描述符
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		if (fs_struct->fdList[i] != -1) {
			freeFd(fs_struct->fdList[i]);
		}
	}

	if (fs_struct->cwd_dirent != NULL) {
		file_close(fs_struct->cwd_dirent);
	}
}

/**
 * @brief 获取cwd对应的dirent。如果已获取，则无需重复获取，以免无法释放
 */
Dirent *get_cwd_dirent(thread_fs_t *fs_struct) {
	if (fs_struct->cwd_dirent != NULL) {
		return fs_struct->cwd_dirent;
	} else {
		fs_struct->cwd_dirent = getFile(NULL, fs_struct->cwd);
		return fs_struct->cwd_dirent;
	}
}
