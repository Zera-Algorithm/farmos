#include <fs/cluster.h>
#include <fs/dirent.h>
#include <fs/fat32.h>
#include <fs/file_device.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lock/mutex.h>

/**
 * @brief mtx_file是负责维护磁盘file访问互斥性的锁。
 * 每次我们携带dirent指针进入文件系统层对文件进行操作，都需要获取这个锁，以保证对dirent的
 * 读、写、删除等是互斥的。之前曾设计过更细粒度的锁（对每个Dirent加锁），但因太复杂而使用此粗粒度的锁。
 */
mutex_t mtx_file;

/**
 * 管理文件相关事务
 * 文件新建、读写、删除
 */

struct FileDev file_dev_file = {
    .dev_id = 'f',
    .dev_name = "file",
    .dev_read = file_read,
    .dev_write = file_write,
};

/**
 * @brief 打开路径为path的文件或目录，返回描述项Dirent。每次get引用计数加1，close引用计数减一
 * @param baseDir 文件或目录寻址时的基地址
 * @param path
 * 文件或目录的路径。如果path是绝对路径，则忽略baseDir；如果path。path指向的地址要求为内核地址
 * @return NULL表示失败
 */
struct Dirent *getFile(struct Dirent *baseDir, char *path) {
	mtx_lock_sleep(&mtx_file);

	Dirent *file;
	longEntSet longSet;
	FileSystem *fs;

	extern FileSystem *fatFs;

	if (baseDir) {
		fs = baseDir->file_system;
	} else {
		fs = fatFs;
	}

	int r = walk_path(fs, path, baseDir, 0, &file, 0, &longSet);
	if (r < 0) {
		mtx_unlock_sleep(&mtx_file);
		return NULL;
	} else {
		mtx_unlock_sleep(&mtx_file);
		return file;
	}
}

/**
 * @brief 关闭Dirent，使其引用计数减一
 */
void file_close(Dirent *file) {
	mtx_lock_sleep(&mtx_file);
	dput_path(file);
	mtx_unlock_sleep(&mtx_file);
}

/**
 * @brief 返回文件file第fileClusNo块簇的簇号
 */
static u32 fileGetClusterNo(Dirent *file, int fileClusNo) {
	int clus = file->first_clus;
	for (int i = 0; i <= fileClusNo - 1; i++) {
		clus = fatRead(file->file_system, clus);
	}
	return clus;
}

// 补充两个不获取锁的_file_read_nolock和_file_write_nolock，以供连续写入时使用

/**
 * @brief 将文件 entry 的 off 偏移往后长度为 n 的内容读到 dst 中。如果 user
 * 为真，则为用户地址，否则为内核地址。
 * @return 返回读取文件的字节数
 */
int file_read(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	mtx_lock_sleep(&mtx_file);

	log(LEVEL_MODULE, "read from file %s: off = %d, n = %d\n", file->name, off, n);
	if (off >= file->file_size) {
		// 起始地址超出文件的最大范围

		mtx_unlock_sleep(&mtx_file);
		return -E_EXCEED_FILE;
	} else if (off + n > file->file_size) {
		warn("read too much. shorten read length from %d to %d!\n", n,
		     file->file_size - off);
		n = file->file_size - off;
	}
	assert(n != 0);

	u64 start = off, end = off + n - 1;
	u32 clusSize = file->file_system->superBlock.bytes_per_clus;
	u32 offset = off % clusSize;

	// 寻找第一个cluster
	u32 clusIndex = start / clusSize;
	u32 clus = fileGetClusterNo(file, clusIndex);
	u32 len = 0; // 累计读取的字节数

	// 读取第一块
	clusterRead(file->file_system, clus, offset, (void *)dst, MIN(n, clusSize - offset), user);
	len += MIN(n, clusSize - offset);

	// 之后的块
	clusIndex += 1;
	clus = fatRead(file->file_system, clus);
	for (; end >= clusIndex * clusSize; clusIndex++) {
		clusterRead(file->file_system, clus, 0, (void *)(dst + len), MIN(clusSize, n - len),
			    user);
		clus = fatRead(file->file_system, clus);
		len += MIN(clusSize, n - len);
	}

	mtx_unlock_sleep(&mtx_file);
	return n;
}

/**
 * @brief 扩充文件到新的大小
 */
static void fileExtend(struct Dirent *file, int newSize) {
	assert(file->file_size < newSize);

	file->file_size = newSize;
	FileSystem *fs = file->file_system;

	u32 clusSize = CLUS_SIZE(file->file_system);
	u32 clusIndex = 0;
	u32 clus = file->first_clus;
	for (; FAT32_NOT_END_CLUSTER(fatRead(fs, clus)); clusIndex += 1) {
		clus = fatRead(fs, clus);
	}

	for (; newSize > (clusIndex + 1) * clusSize; clusIndex += 1) {
		clus = clusterAlloc(fs, clus);
	}

	// 写回目录项
	sync_dirent_rawdata_back(file);
}

/**
 * @brief 将 src 写入文件 entry 的 off 偏移往后长度为 n 的内容。如果 user
 * 为真，则为用户地址，否则为内核地址。
 * @note 允许写入的内容超出文件，此时将扩展文件
 * @return 返回写入文件的字节数
 */
int file_write(struct Dirent *file, int user, u64 src, uint off, uint n) {
	mtx_lock_sleep(&mtx_file);

	log(LEVEL_GLOBAL, "write file: %s\n", file->name);
	assert(n != 0);

	// Note: 支持off在任意位置的写入（允许超过file->size），[file->size, off)的部分将被填充为0
	if (off + n > file->file_size) {
		// 超出文件的最大范围
		// Note: 扩充
		fileExtend(file, off + n);
	}

	u64 start = off, end = off + n - 1;
	u32 clusSize = file->file_system->superBlock.bytes_per_clus;
	u32 offset = off % clusSize;

	// 寻找第一个cluster
	u32 clusIndex = start / clusSize;
	u32 clus = fileGetClusterNo(file, clusIndex);
	u32 len = 0; // 累计读取的字节数

	// 读取第一块
	clusterWrite(file->file_system, clus, offset, (void *)src, MIN(n, clusSize - offset), user);
	len += MIN(n, clusSize - offset);

	// 之后的块
	clusIndex += 1;
	clus = fatRead(file->file_system, clus);
	for (; end >= clusIndex * clusSize; clusIndex++) {
		clusterWrite(file->file_system, clus, 0, (void *)(src + len),
			     MIN(clusSize, n - len), user);
		clus = fatRead(file->file_system, clus);
		len += MIN(clusSize, n - len);
	}

	mtx_unlock_sleep(&mtx_file);
	return n;
}

#define ROUNDUP(a, x) (((a) + (x)-1) & ~((x)-1))
/**
 * @brief 获取文件状态信息
 * @param kstat 内核态指针，指向文件信息结构体
 */
void fileStat(struct Dirent *file, struct kstat *pKStat) {
	mtx_lock_sleep(&mtx_file);

	memset(pKStat, 0, sizeof(struct kstat));
	// P262 Linux-Unix系统编程手册
	pKStat->st_dev = file->file_system->deviceNumber;
	pKStat->st_ino = 0;   // 并未实现inode
	pKStat->st_mode = 0;  // 未实现
	pKStat->st_nlink = 1; // 文件的链接数，无链接时为1
	pKStat->st_uid = 0;
	pKStat->st_gid = 0;
	pKStat->st_rdev = 0;
	pKStat->st_size = file->file_size;
	pKStat->st_blksize = CLUS_SIZE(file->file_system);
	pKStat->st_blocks = ROUNDUP(file->file_size, pKStat->st_blksize);

	// 时间相关
	pKStat->st_atime_sec = 0;
	pKStat->st_atime_nsec = 0;
	pKStat->st_mtime_sec = 0;
	pKStat->st_mtime_nsec = 0;
	pKStat->st_ctime_sec = 0;
	pKStat->st_ctime_nsec = 0;

	mtx_unlock_sleep(&mtx_file);
}
