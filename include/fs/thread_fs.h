#ifndef _THREAD_FS_H
#define _THREAD_FS_H

#include <fs/fs.h>
#include <lock/mutex.h>
#include <types.h>

#ifndef MAX_FD_COUNT
#define MAX_FD_COUNT 256
#endif

typedef struct Dirent Dirent;

typedef struct mutex mutex_t;

typedef struct thread_fs {
	mutex_t lock;
	int fdList[MAX_FD_COUNT];
	char cwd[MAX_NAME_LEN];
	Dirent *cwd_dirent;
	u64 mmap_addr;	      // mmap用到的地址位置
	u64 rlimit_files_cur; // 进程最大打开文件数soft limit
	u64 rlimit_files_max; // 进程最大打开文件数hard limit
} thread_fs_t;

void init_thread_fs(thread_fs_t *td_fs_struct);
void fork_thread_fs(thread_fs_t *old, thread_fs_t *new);
void recycle_thread_fs(thread_fs_t *td_fs_struct);

Dirent *get_cwd_dirent(thread_fs_t *td_fs_struct);

#endif
