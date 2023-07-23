#include <fs/cluster.h>
#include <fs/dirent.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lock/mutex.h>

extern mutex_t mtx_file;

/**
 * @brief 创建链接
 * @param oldDir 旧目录的Dirent。如果oldPath使用绝对路径，可以将该参数置为NULL
 * @param oldPath 相对于旧目录的文件路径。如果为绝对路径，则可以忽略oldDir
 * @param newDir 新目录的Dirent
 * @param newPath 相对于新目录的链接路径
 */
int linkat(struct Dirent *oldDir, char *oldPath, struct Dirent *newDir, char *newPath) {
	mtx_lock_sleep(&mtx_file);

	Dirent *oldFile, *newFile;
	if ((oldFile = getFile(oldDir, oldPath)) == NULL) {
		warn("oldFile %d not found!\n", oldPath);

		mtx_unlock_sleep(&mtx_file);
		return -1;
	}

	char path[MAX_NAME_LEN];
	dirent_get_path(oldFile, path);

	unwrap(createFile(newDir, newPath, &newFile));
	newFile->raw_dirent.DIR_Attr |= ATTR_LINK;
	sync_dirent_rawdata_back(newFile);

	oldFile->linkcnt += 1;
	int r = file_write(newFile, 0, (u64)path, 0, strlen(path) + 1);

	mtx_unlock_sleep(&mtx_file);
	return r;
}

/**
 * @brief 递归地从文件的尾部释放其cluster
 * @note TODO: 可能有溢出风险
 */
static void recur_free_clus(FileSystem *fs, int clus, int prev_clus) {
	int next_clus = fatRead(fs, clus);
	if (!FAT32_NOT_END_CLUSTER(next_clus)) {
		// clus is end cluster
		clusterFree(fs, clus, prev_clus);
	} else {
		recur_free_clus(fs, next_clus, clus);
		clusterFree(fs, clus, prev_clus);
	}
}

/**
 * @brief 删除文件
 */
static int rmfile(struct Dirent *file) {
	char linked_file_path[MAX_NAME_LEN];

	int cnt = get_entry_count_by_name(file->name);
	char data = 0xE5;

	if (file->refcnt > 1) {
		warn("other process uses this file! refcnt = %d\n", file->refcnt);
		return -1;
	}

	// 1. 如果是链接文件，则减去其链接数
	if (file->raw_dirent.DIR_Attr & ATTR_LINK) {
		assert(file->file_size < MAX_NAME_LEN);
		file_read(file, 0, (u64)linked_file_path, 0, MAX_NAME_LEN);

		Dirent *linked_file = getFile(NULL, linked_file_path);
		assert(linked_file != NULL);
		linked_file->linkcnt -= 1;
		file_close(linked_file);
	}

	// 2. 断开父子关系
	assert(file->parent_dirent != NULL); // 不处理根目录的情况
	// 先递归删除子Dirent（由于存在意向锁，因此这些）
	if (file->type == DIRENT_DIR) {
		Dirent *tmp;
		LIST_FOREACH (tmp, &file->child_list, dirent_link) {
			rmfile(tmp);
		}
	}
	LIST_REMOVE(file, dirent_link); // 从父亲的子Dirent列表删除

	// 3. 清空目录项
	for (int i = 0; i < cnt; i++) {
		panic_on(file_write(file->parent_dirent, 0, (u64)&data,
				    file->parent_dir_off - i * DIR_SIZE, 1) < 0);
	}

	// 4. 释放其占用的Cluster
	int clus = file->first_clus;
	recur_free_clus(file->file_system, clus, 0);

	dirent_dealloc(file); // 释放目录项
	return 0;
}

/**
 * @brief 撤销链接。即删除(链接)文件
 */
int unlinkat(struct Dirent *dir, char *path) {
	mtx_lock_sleep(&mtx_file);

	Dirent *file;
	if ((file = getFile(dir, path)) == NULL) {
		warn("file %d not found!\n", path);
		mtx_unlock_sleep(&mtx_file);
		return -1;
	}
	rmfile(file);

	mtx_unlock_sleep(&mtx_file);
	return 0;
}
