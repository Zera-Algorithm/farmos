#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <lib/error.h>
#include <lib/printf.h>
#include <lib/string.h>

// 最多挂载16个文件系统
static struct FileSystem fs[MAX_FS_COUNT];

/**
 * @brief 分配一个文件系统结构体
 */
void allocFs(struct FileSystem **pFs) {
	for (int i = 0; i < MAX_FS_COUNT; i++) {
		if (fs[i].valid == 0) {
			*pFs = &fs[i];
			memset(&fs[i], 0, sizeof(FileSystem));
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

FileSystem *findFsBy(findfs_callback_t findfs, void *data) {
	for (int i = 0; i < MAX_FS_COUNT; i++) {
		if (findfs(&fs[i], data)) {
			return &fs[i];
		}
	}
	return NULL;
}
