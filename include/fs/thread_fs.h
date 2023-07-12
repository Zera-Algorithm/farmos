#ifndef _THREAD_FS_H
#define _THREAD_FS_H

#include <fs/fs.h>
#include <types.h>

#ifndef MAX_FD_COUNT
#define MAX_FD_COUNT 256
#endif

typedef struct Dirent Dirent;

typedef struct thread_fs {
	int fdList[MAX_FD_COUNT];
	char cwd[MAX_NAME_LEN];
	Dirent *cwd_dirent;
} thread_fs_t;

void init_thread_fs(thread_fs_t *td_fs_struct);
void fork_thread_fs(thread_fs_t *old, thread_fs_t *new);
void recycle_thread_fs(thread_fs_t *td_fs_struct);

Dirent *get_cwd_dirent(thread_fs_t *td_fs_struct);

#endif
