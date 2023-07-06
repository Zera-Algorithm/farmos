#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <lib/error.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lock/mutex.h>

// 最多挂载16个文件系统
// FS是一个只读结构，在分配并第一次初始化之后，至回收之前，都不会修改
// 所以不需要设置单个FS的锁，只需要设置FS的整体锁
static struct FileSystem fs[MAX_FS_COUNT];

// FS分配锁
struct mutex mtx_fs;

static Buffer *getBlock(FileSystem *fs, u64 blockNum) {
	if (fs->image == NULL) {
		// 是挂载了根设备，直接读取块缓存层的数据即可
		return bufRead(fs->deviceNumber, blockNum);
	} else {
		// 处理挂载了文件的情况
		Dirent *img = fs->image;
		FileSystem *parentFs = fs->image->file_system;
		int blockNo = fileBlockNo(parentFs, img->first_clus, blockNum);
		return bufRead(parentFs->deviceNumber, blockNo);
	}
}

/**
 * @brief 分配一个文件系统结构体
 */
void allocFs(struct FileSystem **pFs) {
	mtx_lock(&mtx_fs);

	for (int i = 0; i < MAX_FS_COUNT; i++) {
		if (fs[i].valid == 0) {
			*pFs = &fs[i];
			memset(&fs[i], 0, sizeof(FileSystem));
			fs[i].valid = 1;
			fs[i].get = getBlock;

			mtx_unlock(&mtx_fs);
			return;
		}
	}
	panic("No more fs to alloc!");
}

/**
 * @brief 释放一个文件系统结构体
 */
void deAllocFs(struct FileSystem *fs) {
	mtx_lock(&mtx_fs);
	fs->valid = 0;
	memset(fs, 0, sizeof(struct FileSystem));
	mtx_unlock(&mtx_fs);
}

FileSystem *find_fs_by(findfs_callback_t findfs, void *data) {
	mtx_lock(&mtx_fs);
	for (int i = 0; i < MAX_FS_COUNT; i++) {
		if (findfs(&fs[i], data)) {
			mtx_unlock(&mtx_fs);
			return &fs[i];
		}
	}
	mtx_unlock(&mtx_fs);
	return NULL;
}

int find_fs_of_dir(FileSystem *fs, void *data) {
	Dirent *dir = (Dirent *)data;
	if (fs->mountPoint == NULL) {
		return 0;
	} else {
		return fs->mountPoint->first_clus == dir->first_clus;
	}
}
