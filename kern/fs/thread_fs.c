/**
 * @brief 本文件规定与thread的通用接口，包括init, fork, recycle三个动作时对进程管理的fs结构的处理
 * 本结构体的互斥场景仅仅需要考虑同一进程不同线程的互斥即可
 */

#include <fs/console.h>
#include <fs/fd.h>
#include <fs/file.h>
#include <fs/thread_fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/string.h>
#include <lock/mutex.h>

void init_thread_fs(thread_fs_t *td_fs_struct) {
	strncpy(td_fs_struct->cwd, "/", 2);

	// 初始化文件描述符表
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		td_fs_struct->fdList[i] = -1;
	}

	// 初始化控制台文件描述符
	td_fs_struct->fdList[0] = readConsoleAlloc();
	td_fs_struct->fdList[1] = writeConsoleAlloc();
	td_fs_struct->fdList[2] = errorConsoleAlloc();

	td_fs_struct->cwd_dirent = NULL;

	// 初始化MMAP区域的开始位置
	td_fs_struct->mmap_addr = MMAP_START;

	td_fs_struct->rlimit_files_cur = MAX_FD_COUNT;
	td_fs_struct->rlimit_files_max = MAX_FD_COUNT;

	// 初始化进程fs结构体的锁为自旋锁
	mtx_init(&td_fs_struct->lock, "thread_fs_lock", true, MTX_SPIN);
}

// 不设置cwd_dirent是为了在get_cwd_dirent时设置
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

	// 继承文件数的限制
	new->rlimit_files_cur = old->rlimit_files_cur;
	new->rlimit_files_max = old->rlimit_files_max;

	// 初始化进程fs结构体的锁为自旋锁
	mtx_init(&new->lock, "thread_fs_lock", true, MTX_SPIN);
}

// 在进程结束时回收进程的fs结构体
void recycle_thread_fs(thread_fs_t *td_fs_struct) {
	td_fs_struct->cwd[0] = 0;

	// 回收进程的文件描述符
	for (int i = 0; i < MAX_FD_COUNT; i++) {
		if (td_fs_struct->fdList[i] != -1) {
			freeFd(td_fs_struct->fdList[i]);
		}
	}

	if (td_fs_struct->cwd_dirent != NULL) {
		file_close(td_fs_struct->cwd_dirent);
	}
}

/**
 * @brief 获取cwd对应的dirent。如果已获取，则无需重复获取，以免无法释放
 * @todo 加锁，以及chdir时的变更
 */
Dirent *get_cwd_dirent(thread_fs_t *td_fs_struct) {
	if (td_fs_struct->cwd_dirent != NULL) {
		return td_fs_struct->cwd_dirent;
	} else {
		// 我们保证正常情况下不会panic，因为chroot的时候会检查cwd是否有效
		panic_on(getFile(NULL, td_fs_struct->cwd, &(td_fs_struct->cwd_dirent)));
		return td_fs_struct->cwd_dirent;
	}
}
