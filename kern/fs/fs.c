#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <lib/error.h>
#include <lib/printf.h>
#include <lib/string.h>

#define MAX_FS_COUNT 16

// 最多挂载16个文件系统
static struct FileSystem fs[MAX_FS_COUNT];

/**
 * @brief 分配一个文件系统结构体
 */
void allocFs(struct FileSystem **pFs) {
	for (int i = 0; i < MAX_FS_COUNT; i++) {
		if (fs[i].valid == 0) {
			*pFs = &fs[i];
			fs[i].valid = 1;
			return;
		}
	}
	panic("No more fs to alloc!");
}

/**
 * @brief 释放一个文件系统结构体
 */
void deAllocFs(struct FileSystem *fs) {
	fs->valid = 0;
	memset(fs, 0, sizeof(struct FileSystem));
}
