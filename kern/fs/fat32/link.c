#include <fs/cluster.h>
#include <fs/dirent.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lock/mutex.h>
#include <mm/kmalloc.h>
#include <mm/vmm.h>
#include <sys/errno.h>

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
		return -ENOENT;
	}

	char path[MAX_NAME_LEN];
	dirent_get_path(oldFile, path);

	unwrap(createFile(newDir, newPath, &newFile));
	newFile->raw_dirent.DIR_Attr |= ATTR_LINK;
	sync_dirent_rawdata_back(newFile);

	oldFile->linkcnt += 1;
	int r = file_write(newFile, 0, (u64)path, 0, strlen(path) + 1);

	mtx_unlock_sleep(&mtx_file);
	if (r != strlen(path) + 1) {
		warn("linkat: write path %s to link file %s failed!\n", path, newPath);
		return -EIO; // 暂时想不出其他返回值了就返回这个吧
	} else {
		return 0;
	}
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
 * @brief 删除文件。支持递归删除文件夹
 */
static int rmfile(struct Dirent *file) {
	char linked_file_path[MAX_NAME_LEN];

	int cnt = get_entry_count_by_name(file->name);
	char data = 0xE5;

	if (file->refcnt > 1) {
		warn("other process uses file %s! refcnt = %d\n", file->name, file->refcnt);
		return -EBUSY; // in use
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
	// 先递归删除子Dirent（由于存在意向锁，因此这样）
	if (file->type == DIRENT_DIR) {
		Dirent *tmp;
		LIST_FOREACH (tmp, &file->child_list, dirent_link) { rmfile(tmp); }
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
		warn("file %s not found!\n", path);
		mtx_unlock_sleep(&mtx_file);
		return -ENOENT;
	}
	rmfile(file);

	mtx_unlock_sleep(&mtx_file);
	return 0;
}

/**
 * @brief 将srcfile中的内容写入到（空的）dst_file中
 */
static void file_transfer(Dirent *src_file, Dirent *dst_file) {
	void *buf = (void *)kvmAlloc();

	u64 size = src_file->file_size;
	u64 r1, r2;
	for (u64 i = 0; i < size; i += PAGE_SIZE) {
		u64 cur_size = MIN(size - i, PAGE_SIZE);
		r1 = file_read(src_file, 0, (u64)buf, i, cur_size);
		r2 = file_write(dst_file, 0, (u64)buf, i, cur_size);
		assert(r1 == cur_size);
		assert(r2 == cur_size);
	}

	kvmFree((u64)buf);
}

// 为了支持mount到FATfs的文件互拷贝，采用复制-删除的模式
static int mvfile(Dirent *oldfile, Dirent *newDir, char *newPath) {
	int ret;
	if (oldfile->refcnt > 1) {
		warn("other process uses this file! refcnt = %d\n", oldfile->refcnt);

#ifdef REFCNT_DEBUG
		for (int i = 0; i < oldfile->holder_cnt; i++) {
			warn("holder: %s\n", oldfile->holders[i]);
		}
#endif

		return -EBUSY; // in use
	}

	Dirent *newfile;
	if (oldfile->type == DIRENT_FILE) {
		// 1. 在新目录中尝试新建
		unwrap(createFile(newDir, newPath, &newfile));
		file_transfer(oldfile, newfile);

		// 2. 从原目录中删除
		return rmfile(oldfile);
	} else if (oldfile->type == DIRENT_DIR) {
		// 1. 创建同名的目录
		unwrap(makeDirAt(newDir, newPath, 0));

		// 2. 遍历oldFile目录下的文件，递归，如果中途有错误，就立刻返回
		// 名称newPath由kmalloc分配，记得释放（之所以不在栈上是为了防止溢出）
		Dirent *child;
		LIST_FOREACH (child, &oldfile->child_list, dirent_link) {
			char *new_child_path = kmalloc(MAX_NAME_LEN);
			strncpy(new_child_path, newPath, MAX_NAME_LEN);
			strcat(new_child_path, "/");
			strcat(new_child_path, child->name);

			// 递归
			ret = mvfile(child, newDir, new_child_path);
			kfree(new_child_path);

			if (ret < 0) {
				warn("mvfile: mv child %s failed! (partially move)\n", child->name);
				return ret;
			}
		}

		// 3. 删除旧的目录
		return rmfile(oldfile);
	} else {
		panic("unknown dirent type: %d\n", oldfile->type);
	}
	return 0;
}

// TODO: 处理flags
int renameat2(Dirent *oldDir, char *oldPath, Dirent *newDir, char *newPath, u32 flags) {
	mtx_lock_sleep(&mtx_file);

	// 1. 前置的检查，获取oldFile
	Dirent *oldFile, *newFile;
	if ((oldFile = getFile(oldDir, oldPath)) == NULL) {
		warn("renameat2: oldFile %s not found!\n", oldPath);
		mtx_unlock_sleep(&mtx_file);
		return -ENOENT;
	}

	if ((newFile = getFile(newDir, newPath)) != NULL) {
		warn("renameat2: newFile %s exists!\n", newPath);
		file_close(newFile);
		mtx_unlock_sleep(&mtx_file);
		return -EEXIST;
	}

	int ret = mvfile(oldFile, newDir, newPath);
	mtx_unlock_sleep(&mtx_file);
	return ret;
}
