#ifndef _SYSCALL_MMAP_H
#define _SYSCALL_MMAP_H

#include <lib/queue.h>
#include <types.h>

// 记录mmap文件信息的结构体
typedef struct mmap_fd_info {
	int fd;
	u64 offset;
	u64 addr;
	u64 length;

	LIST_ENTRY(mmap_fd_info) mmap_entry;
} mmap_fd_info_t;

LIST_HEAD(mmap_fd_info_list, mmap_fd_info);

typedef struct mmap_fd_info_list mmap_fd_info_list_t;

#endif
