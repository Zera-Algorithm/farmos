#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/dirent.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/wchar.h>

#define MAX_CLUS_SIZE (128 * BUF_SIZE)

struct FileSystem *fatFs;
static void writeBackDirent(Dirent *dirent);
static int countClusters(struct Dirent *file);

static Buffer *getBlock(FileSystem *fs, u64 blockNum) {
	if (fs->image == NULL) {
		// 是挂载了根设备，直接读取块缓存层的数据即可
		return bufRead(fs->deviceNumber, blockNum);
	}
	// TODO: 处理挂载了文件的情况

	panic("unimplemented");
	return NULL;
}

void fat32Init() {
	log(LEVEL_GLOBAL, "fat32 is initing...\n");
	FileSystem *fs;
	allocFs(&fs);

	fs->image = NULL;
	fs->deviceNumber = 0;
	fs->get = getBlock;
	strncpy(fs->name, "FAT32", 8);
	panic_on(clusterInit(fs));

	log(LEVEL_GLOBAL, "cluster Init Finished!\n");
	direntInit();

	// 初始化根目录
	fs->root.firstClus = fs->superBlock.bpb.root_clus;
	log(LEVEL_GLOBAL, "first clus of root is %d\n", fs->root.firstClus);

	fs->root.rawDirEnt.DIR_Attr = ATTR_DIRECTORY;
	strncpy(fs->root.name, "/", 2);
	log(LEVEL_GLOBAL, "set dir_attr\n");

	fs->root.rawDirEnt.DIR_FileSize = 0; // 目录的Dirent的size都是0
	log(LEVEL_GLOBAL, "DIR_fileSize\n");

	fs->root.parentDirent = NULL; // 父节点为空，表示已经到达根节点
	// 此句必须放在countCluster之前，用于设置fs
	fs->root.fileSystem = fs;

	log(LEVEL_GLOBAL, "before count cluster\n");
	fs->root.fileSize = countClusters(&(fs->root)) * CLUS_SIZE(fs);

	log(LEVEL_GLOBAL, "root directory init finished!\n");

	assert(sizeof(FAT32Directory) == DIRENT_SIZE);
	fatFs = fs;

	log(LEVEL_GLOBAL, "fat32 init finished!\n");
}

// 簇缓冲区：簇最大为128个BUF_SIZE
static char clusBuf[MAX_CLUS_SIZE];

/**
 * @brief 计数文件的簇数
 */
static int countClusters(struct Dirent *file) {
	log(LEVEL_GLOBAL, "count Cluster begin!\n");

	int clus = file->firstClus;
	int i = 0;
	if (clus == 0) {
		log(LEVEL_GLOBAL, "cluster is 0!\n");
		return 0;
	}
	// 如果文件不包含任何块，则直接返回0即可。
	else {
		while (FAT32_NOT_END_CLUSTER(clus)) {
			log(LEVEL_GLOBAL, "clus is %d\n", clus);
			clus = fatRead(file->fileSystem, clus);
			i += 1;
		}
		log(LEVEL_GLOBAL, "count Cluster end!\n");
		return i;
	}
}

/**
 * @param offset 开始查询的位置偏移
 * @param next_offset 下一个dirent的开始位置
 * @return 读取的内容长度。若为0，表示读到末尾
 */
int dirGetDentFrom(Dirent *dir, u64 offset, struct Dirent **file, int *next_offset,
		   longEntSet *longSet) {
	assert(offset % DIR_SIZE == 0);

	FileSystem *fs = dir->fileSystem;
	u32 j;
	FAT32Directory *f;
	FAT32LongDirectory *longEnt;
	int clusSize = CLUS_SIZE(fs);

	char tmpName[MAX_NAME_LEN];
	char tmpBuf[32] __attribute__((aligned(2)));
	wchar fullName[MAX_NAME_LEN]; // 以wchar形式储存的文件名
	fullName[0] = 0;	      // 初始化为空字符串

	if (longSet)
		longSet->cnt = 0; // 初始化longSet有0个元素

	// 遍历所有dir中的项目
	for (j = offset; j < dir->fileSize; j += DIR_SIZE) {
		fileRead(dir, 0, (u64)clusBuf, j, DIR_SIZE);
		f = ((FAT32Directory *)clusBuf);

		// 跳过空项（0xE5表示已删除）
		if (f->DIR_Name[0] == 0 || f->DIR_Name[0] == 0xE5)
			continue;

		// 是长文件名项（可能属于文件也可能属于目录）
		if (f->DIR_Attr & ATTR_LONG_NAME_MASK) {

			longEnt = (FAT32LongDirectory *)f;
			// 是第一项
			if (longEnt->LDIR_Ord & LAST_LONG_ENTRY) {
				tmpName[0] = 0;
				if (longSet)
					longSet->cnt = 0;
			}

			// 向longSet里面存放长文件名项的指针
			if (longSet)
				longSet->longEnt[longSet->cnt++] = longEnt;
			memcpy(tmpBuf, (char *)longEnt->LDIR_Name1, 10);
			memcpy(tmpBuf + 10, (char *)longEnt->LDIR_Name2, 12);
			memcpy(tmpBuf + 22, (char *)longEnt->LDIR_Name3, 4);
			wstrnins(fullName, (const wchar *)tmpBuf, 13);
		} else {
			if (wstrlen(fullName) != 0) {
				wstr2str(tmpName, fullName);
			} else {
				strncpy(tmpName, (const char *)f->DIR_Name, 11);
				tmpName[11] = 0;
			}

			log(DEBUG, "find: \"%s\"\n", tmpName);
			// writef("size = %d\n", sizeof(FAT32LongDirectory));

			// TODO: 此为权益之计
			Dirent *dirent = direntAlloc();
			dirent->rawDirEnt = *f;
			dirent->fileSystem = fs;
			dirent->firstClus = f->DIR_FstClusHI * 65536 + f->DIR_FstClusLO;
			dirent->fileSize = f->DIR_FileSize;
			dirent->parentDirent = dir; // 设置父级目录项
			dirent->off = j;	    // 父亲目录内偏移
			strncpy(dirent->name, tmpName, MAX_NAME_LEN);

			LIST_INIT(&dirent->childList);
			// 对于目录文件的大小，我们将其重置为其簇数乘以簇大小，不再是0
			if (dirent->rawDirEnt.DIR_Attr & ATTR_DIRECTORY) {
				dirent->fileSize = countClusters(dirent) * clusSize;
			}

			*file = dirent;
			*next_offset = j + DIR_SIZE;
			return DIR_SIZE;
		}
	}

	warn("no more dents in dir: %s\n", dir->name);
	*next_offset = dir->fileSize;
	return 0; // 读到结尾
}

/**
 * @brief 在dir中找一个名字为name的文件，支持长文件名
 * @note 少数几个能扫描目录项获得 Dirent 的函数
 * @param longEntSet 返回的长文件名信息
 */
static int dirLookup(FileSystem *fs, Dirent *dir, char *name, struct Dirent **file,
		     longEntSet *longSet) {
	// TODO: 使用dirGetDentFrom函数重写此函数
	u32 j;
	FAT32Directory *f;
	FAT32LongDirectory *longEnt;
	int clusSize = CLUS_SIZE(fs);

	char tmpName[MAX_NAME_LEN];
	char tmpBuf[32] __attribute__((aligned(2)));
	wchar fullName[MAX_NAME_LEN]; // 以wchar形式储存的文件名
	fullName[0] = 0;	      // 初始化为空字符串

	longSet->cnt = 0; // 初始化longSet有0个元素

	// 遍历所有dir中的项目
	for (j = 0; j < dir->fileSize; j += DIR_SIZE) {
		fileRead(dir, 0, (u64)clusBuf, j, DIR_SIZE);
		f = ((FAT32Directory *)clusBuf);

		// 跳过空项（0xE5表示已删除）
		if (f->DIR_Name[0] == 0 || f->DIR_Name[0] == 0xE5)
			continue;

		// 是长文件名项（可能属于文件也可能属于目录）
		if (f->DIR_Attr & ATTR_LONG_NAME_MASK) {

			longEnt = (FAT32LongDirectory *)f;
			// 是第一项
			if (longEnt->LDIR_Ord & LAST_LONG_ENTRY) {
				tmpName[0] = 0;
				longSet->cnt = 0;
			}

			// 向longSet里面存放长文件名项的指针
			longSet->longEnt[longSet->cnt++] = longEnt;
			memcpy(tmpBuf, (char *)longEnt->LDIR_Name1, 10);
			memcpy(tmpBuf + 10, (char *)longEnt->LDIR_Name2, 12);
			memcpy(tmpBuf + 22, (char *)longEnt->LDIR_Name3, 4);
			wstrnins(fullName, (const wchar *)tmpBuf, 13);
		} else {
			if (wstrlen(fullName) != 0) {
				wstr2str(tmpName, fullName);
			} else {
				strncpy(tmpName, (const char *)f->DIR_Name, 11);
				tmpName[11] = 0;
			}

			log(DEBUG, "find: \"%s\"\n", tmpName);
			// writef("size = %d\n", sizeof(FAT32LongDirectory));
			if (strncmp(tmpName, name, MAX_NAME_LEN) == 0) {
				// 找到了名称相同的文件

				// TODO: 此为权益之计
				Dirent *dirent = direntAlloc();
				dirent->rawDirEnt = *f;
				dirent->fileSystem = fs;
				dirent->firstClus = f->DIR_FstClusHI * 65536 + f->DIR_FstClusLO;
				dirent->fileSize = f->DIR_FileSize;
				dirent->parentDirent = dir; // 设置父级目录项
				dirent->off = j;	    // 父亲目录内偏移
				strncpy(dirent->name, tmpName, MAX_NAME_LEN);

				LIST_INIT(&dirent->childList);
				// 对于目录文件的大小，我们将其重置为其簇数乘以簇大小，不再是0
				if (dirent->rawDirEnt.DIR_Attr & ATTR_DIRECTORY) {
					dirent->fileSize = countClusters(dirent) * clusSize;
				}

				*file = dirent;
				return 0;
			} else {
				// 清空长文件名缓冲区，以待下一次写入
				fullName[0] = 0;
				longSet->cnt = 0;
			}
		}
	}

	log(LEVEL_GLOBAL, "File \"%s\" Not found!\n", name);
	return -E_NOT_FOUND;
}

/**
 * @brief 跳过左斜线。unix传统，允许路径上有连续的多个左斜线，解析时看作一条
 */
static char *skipSlash(char *p) {
	while (*p == '/') {
		p++;
	}
	return p;
}

/**
 * @brief 遍历路径，找到某个文件
 * @param baseDir 开始遍历的根，为 NULL 表示忽略。但如果path以'/'开头，则强制使用根目录
 * @param pdir 文件所在的目录
 * @param pfile 文件本身
 * @param lastelem 如果恰好找到了文件的上一级目录的位置，则返回最后未匹配的那个项目的名称(legacy)
 */
// TODO: 动态根据Dirent识别 fs，以及引入链接和虚拟文件的机制
static int walkPath(FileSystem *fs, char *path, Dirent *baseDir, Dirent **pdir, Dirent **pfile,
		    char *lastelem, longEntSet *longSet) {
	char *p;
	char name[MAX_NAME_LEN];
	Dirent *dir, *file;
	int r;

	// 计算初始Dirent
	if (path[0] == '/' || baseDir == NULL) {
		file = &fs->root;
	} else {
		file = baseDir;
	}

	path = skipSlash(path);
	dir = NULL;
	name[0] = 0;
	*pfile = 0;

	while (*path != '\0') {
		dir = file;
		p = path;

		// 1. 循环读取文件名
		while (*path != '/' && *path != '\0') {
			path++;
		}

		if (path - p >= MAX_NAME_LEN) {
			return -E_BAD_PATH;
		}

		memcpy(name, p, path - p);
		name[path - p] = '\0';

		path = skipSlash(path);

		// 2. 检查目录的属性
		// 如果不是目录，则直接报错
		if (!(dir->rawDirEnt.DIR_Attr & ATTR_DIRECTORY)) {
			return -E_NOT_FOUND;
		}

		if (strncmp(name, ".", 2) == 0) {
			continue;
		} else if (strncmp(name, "..", 3) == 0) {
			dir = dir->parentDirent;
			file = file->parentDirent;
			continue;
		}

		// 3. 继续遍历目录
		if ((r = dirLookup(fs, dir, name, &file, longSet)) < 0) {
			// printf("r = %d\n", r);
			// *path == '\0'表示遍历到最后一个项目了
			if (r == -E_NOT_FOUND && *path == '\0') {
				if (pdir) {
					*pdir = dir;
				}

				if (lastelem) {
					strncpy(lastelem, name, MAX_NAME_LEN);
				}

				*pfile = 0;
			}
			return r;
		}
	}

	if (pdir) {
		*pdir = dir;
	}
	*pfile = file;
	return 0;
}

/**
 * @brief 打开路径为path的文件或目录，返回描述项Dirent
 * @param baseDir 文件或目录寻址时的基地址
 * @param path
 * 文件或目录的路径。如果path是绝对路径，则忽略baseDir；如果path。path指向的地址要求为内核地址
 * @return NULL表示失败
 */
struct Dirent *getFile(struct Dirent *baseDir, char *path) {
	Dirent *file;
	longEntSet longSet;
	FileSystem *fs;
	if (baseDir) {
		fs = baseDir->fileSystem;
	} else {
		fs = fatFs;
	}

	int r = walkPath(fs, path, baseDir, 0, &file, 0, &longSet);
	if (r < 0) {
		return NULL;
	} else {
		return file;
	}
}

/**
 * @brief 在某个目录中新建一个目录项Dirent
 * @param dir 要分配文件的目录
 * @param ent 返回的分配了的目录项。如果要求多个，Dirent->off设为最后一项的偏移
 * @param cnt 需要分配的连续目录项数
 */
static int dirAllocEntry(Dirent *dir, Dirent **ent, int cnt) {
	assert(cnt >= 1); // 要求分配的个数不能小于1

	int i, j, lastClus = 0;
	FileSystem *fs = dir->fileSystem;
	u32 clusSize = CLUS_SIZE(fs);
	u32 offset = 0;

	// 1. 找到最后一个簇的簇号lastClus和偏移量offset
	int clus = dir->firstClus;
	for (i = clus; FAT32_NOT_END_CLUSTER(i); i = fatRead(fs, i)) {

		// 如果cnt = 1，就首先寻找已有的块中的空闲项
		if (cnt == 1) {
			clusterRead(fs, i, 0, clusBuf, clusSize, 0);
			FAT32Directory *curList = (FAT32Directory *)clusBuf;
			for (j = 0; j < clusSize / DIRENT_SIZE; j++) {
				if (curList[j].DIR_Name[0] == 0) {
					Dirent *dirent = direntAlloc();
					curList[j].DIR_Name[0] =
					    1; // 染黑，防止后面的分配使用到该块
					// 回写
					clusterWrite(fs, i, 0, clusBuf, clusSize, 0);
					dirent->off = offset + j * DIRENT_SIZE;
					*ent = dirent;
					return 0;
				}
			}
		}

		lastClus = i;
		offset += clusSize;
	}

	// 2. 如果没有找到空余空间，或者需分配的目录项数超过1，
	//    就重新分配一个，然后分配若干个连续的目录项
	u32 newClus = clusterAlloc(fs, lastClus);
	dir->fileSize += clusSize;
	Dirent *dirent = direntAlloc();

	// 3. 将需要用到的目录项染黑，防止后面的分配使用到该块
	assert(cnt <= clusSize / DIR_SIZE);
	clusterRead(fs, newClus, 0, clusBuf, clusSize, 0);
	for (int i = 0; i < cnt; i++) {
		((FAT32Directory *)clusBuf)[i].DIR_Name[0] = 1;
	}
	clusterWrite(fs, newClus, 0, clusBuf, clusSize, 0); // 写回

	// 3. 记录尾部目录项（即记录文件元信息的目录项）的偏移
	dirent->off = offset + DIR_SIZE * (cnt - 1);
	*ent = dirent;
	return 0;
}

/**
 * @brief 填写长文件名项，返回应该继续填的位置。如果已经填完，返回NULL
 */
static char *fillLongEntry(FAT32LongDirectory *longDir, char *raw_name) {
	wchar _name[MAX_NAME_LEN];
	wchar *name = _name;
	str2wstr(name, raw_name);
	int len = strlen(raw_name) + 1;

	longDir->LDIR_Attr = ATTR_LONG_NAME_MASK;
	memcpy((char *)longDir->LDIR_Name1, (char *)name, 10);
	len -= 5, name += 5;
	if (len <= 0) {
		return NULL;
	}

	memcpy((char *)longDir->LDIR_Name2, (char *)name, 12);
	len -= 6, name += 6;
	if (len <= 0) {
		return NULL;
	}

	memcpy((char *)longDir->LDIR_Name3, (char *)name, 4);
	len -= 2, name += 2;
	if (len <= 0) {
		return NULL;
	} else {
		return raw_name + (name - _name);
	}
}

/**
 * @brief 分配一个目录项，其中填入文件名（未来可能支持长文件名）
 */
static int dirAllocFile(Dirent *dir, Dirent **file, char *name) {
	Dirent *dirent = direntAlloc();

	if (strlen(name) > 10) {
		// 需要长文件名
		log(LEVEL_GLOBAL, "create a file using long Name! name is %s\n", name);
		int len = (strlen(name) + 1);
		int cnt = 1; // 包括短文件名项
		if (len % BYTES_LONGENT == 0) {
			cnt += len / BYTES_LONGENT;
		} else {
			cnt += len / BYTES_LONGENT + 1;
		}

		unwrap(dirAllocEntry(dir, &dirent, cnt));
		strncpy(dirent->name, name, MAX_NAME_LEN);
		strncpy((char *)dirent->rawDirEnt.DIR_Name, name, 10);
		dirent->rawDirEnt.DIR_Name[10] = 0;

		// 倒序填写长文件名
		for (int i = 1; i <= cnt - 1; i++) {
			FAT32LongDirectory longDir;

			memset(&longDir, 0, sizeof(FAT32LongDirectory));
			name = fillLongEntry(&longDir, name);
			longDir.LDIR_Ord = i;
			if (i == 1)
				longDir.LDIR_Ord = LAST_LONG_ENTRY;

			// 写入到目录中
			fileWrite(dir, 0, (u64)&longDir, dirent->off - i * DIR_SIZE, DIR_SIZE);

			if (name == NULL) {
				break;
			}
		}
	} else { // 短文件名
		unwrap(dirAllocEntry(dir, &dirent, 1));
		strncpy((char *)dirent->rawDirEnt.DIR_Name, name, 11);
		strncpy(dirent->name, name, 11);
	}

	*file = dirent;
	return 0;
}

/**
 * @brief 将Dirent结构体里的有效数据同步到dirent中，并写回
 */
static void syncDirentRawDataBack(Dirent *dirent) {
	// firstClus, fileSize
	// name不需要，因为在文件创建阶段就已经固定了
	dirent->rawDirEnt.DIR_FstClusHI = dirent->firstClus / 65536;
	dirent->rawDirEnt.DIR_FstClusLO = dirent->firstClus % 65536;
	dirent->rawDirEnt.DIR_FileSize = dirent->fileSize;
	writeBackDirent(dirent);
}

/**
 * @brief 创建一个文件
 */
int r;
int createFile(struct Dirent *baseDir, char *path, Dirent **file) {
	char lastElem[MAX_NAME_LEN];
	Dirent *dir, *f;
	longEntSet longSet;

	if ((r = walkPath(fatFs, path, baseDir, &dir, &f, lastElem, &longSet)) == 0) {
		warn("file exists: %s\n", path);
		return -E_FILE_EXISTS;
	}

	// 出现其他错误，或者没有找到上一级的目录
	if (r != -E_NOT_FOUND || dir == 0) {
		return r;
	}

	unwrap(dirAllocFile(dir, &f, lastElem));

	// 无论如何，创建了的文件至少应当分配一个块
	f->fileSize = 0;
	f->firstClus = clusterAlloc(dir->fileSystem, 0);
	f->parentDirent = dir; // 设置父亲节点，以安排写回
	f->fileSystem = dir->fileSystem;

	syncDirentRawDataBack(f);

	*file = f;
	return 0;
}

/**
 * @brief 返回文件file第fileClusNo块簇的簇号
 */
static u32 fileGetClusterNo(Dirent *file, int fileClusNo) {
	int clus = file->firstClus;
	for (int i = 0; i <= fileClusNo - 1; i++) {
		clus = fatRead(file->fileSystem, clus);
	}
	return clus;
}

/**
 * @brief 将文件 entry 的 off 偏移往后长度为 n 的内容读到 dst 中。如果 user
 * 为真，则为用户地址，否则为内核地址。
 * @return 返回写入文件的字节数
 */
int fileRead(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	log(LEVEL_MODULE, "read from file %s: off = %d, n = %d\n", file->name, off, n);
	if (off >= file->fileSize) {
		// 起始地址超出文件的最大范围
		return -E_EXCEED_FILE;
	} else if (off + n > file->fileSize) {
		warn("read too much. shorten read length from %d to %d!\n", n,
		     file->fileSize - off);
		n = file->fileSize - off;
	}
	assert(n != 0);

	u64 start = off, end = off + n - 1;
	u32 clusSize = file->fileSystem->superBlock.bytes_per_clus;
	u32 offset = off % clusSize;

	// 寻找第一个cluster
	u32 clusIndex = start / clusSize;
	u32 clus = fileGetClusterNo(file, clusIndex);
	u32 len = 0; // 累计读取的字节数

	// 读取第一块
	clusterRead(file->fileSystem, clus, offset, (void *)dst, MIN(n, clusSize - offset), user);
	len += MIN(n, clusSize - offset);

	// 之后的块
	clusIndex += 1;
	clus = fatRead(file->fileSystem, clus);
	for (; end >= clusIndex * clusSize; clusIndex++) {
		clusterRead(file->fileSystem, clus, 0, (void *)(dst + len), MIN(clusSize, n - len),
			    user);
		clus = fatRead(file->fileSystem, clus);
		len += MIN(clusSize, n - len);
	}
	return n;
}

/**
 * @brief 扩充文件到新的大小
 */
static void fileExtend(struct Dirent *file, int newSize) {
	assert(file->fileSize < newSize);

	file->fileSize = newSize;
	FileSystem *fs = file->fileSystem;

	u32 clusSize = CLUS_SIZE(file->fileSystem);
	u32 clusIndex = 0;
	u32 clus = file->firstClus;
	for (; FAT32_NOT_END_CLUSTER(fatRead(fs, clus)); clusIndex += 1) {
		clus = fatRead(fs, clus);
	}

	for (; newSize > (clusIndex + 1) * clusSize; clusIndex += 1) {
		clus = clusterAlloc(fs, clus);
	}

	// 写回目录项
	syncDirentRawDataBack(file);
}

/**
 * @brief 将 src 写入文件 entry 的 off 偏移往后长度为 n 的内容。如果 user
 * 为真，则为用户地址，否则为内核地址。
 * @note 允许写入的内容超出文件，此时将扩展文件
 * @return 返回写入文件的字节数
 */
int fileWrite(struct Dirent *file, int user, u64 src, uint off, uint n) {
	log(LEVEL_GLOBAL, "write file: %s\n", file->name);
	assert(n != 0);
	if (off > file->fileSize) {
		return -E_EXCEED_FILE;
	} else if (off + n > file->fileSize) {
		// 超出文件的最大范围
		// Note: 扩充
		fileExtend(file, off + n);
	}

	u64 start = off, end = off + n - 1;
	u32 clusSize = file->fileSystem->superBlock.bytes_per_clus;
	u32 offset = off % clusSize;

	// 寻找第一个cluster
	u32 clusIndex = start / clusSize;
	u32 clus = fileGetClusterNo(file, clusIndex);
	u32 len = 0; // 累计读取的字节数

	// 读取第一块
	clusterWrite(file->fileSystem, clus, offset, (void *)src, MIN(n, clusSize - offset), user);
	len += MIN(n, clusSize - offset);

	// 之后的块
	clusIndex += 1;
	clus = fatRead(file->fileSystem, clus);
	for (; end >= clusIndex * clusSize; clusIndex++) {
		clusterWrite(file->fileSystem, clus, 0, (void *)(src + len), MIN(clusSize, n - len),
			     user);
		clus = fatRead(file->fileSystem, clus);
		len += MIN(clusSize, n - len);
	}

	return n;
}

// /**
//  * @brief 获取目录的条目
//  * @return 成功执行，返回读取的字节数，失败返回-1
//  */
// int getDents(struct Dirent *dir, struct DirentUser *buf, int len) {
// 	panic("unimplemented");
// 	return 0;
// }

/**
 * @brief 传入一个Dirent，获取其路径
 */
void fileGetPath(Dirent *dirent, char *path) {
	Dirent *tmp = dirent;
	path[0] = 0; // 先把path清空为空串

	if (tmp->parentDirent == NULL) {
		strins(path, "/");
	}
	while (tmp->parentDirent != NULL) {
		strins(path, tmp->name);
		strins(path, "/");
		tmp = dirent->parentDirent;
	}
}

/**
 * @brief 创建链接
 * @param oldDir 旧目录的Dirent。如果oldPath使用绝对路径，可以将该参数置为NULL
 * @param oldPath 相对于旧目录的文件路径。如果为绝对路径，则可以忽略oldDir
 * @param newDir 新目录的Dirent
 * @param newPath 相对于新目录的链接路径
 */
int linkAt(struct Dirent *oldDir, char *oldPath, struct Dirent *newDir, char *newPath) {
	Dirent *oldFile, *newFile;
	if ((oldFile = getFile(oldDir, oldPath)) == NULL) {
		return -1;
	}

	char path[MAX_NAME_LEN];
	fileGetPath(oldFile, path);

	unwrap(createFile(newDir, newPath, &newFile));
	newFile->rawDirEnt.DIR_Attr |= ATTR_LINK;
	writeBackDirent(newFile);

	return fileWrite(newFile, 0, (u64)path, 0, strlen(path) + 1);
}

/**
 * @brief 撤销链接。即删除链接文件
 */
int unLinkAt(struct Dirent *dir, char *path) {
	panic("unimplemented");
	return 0;
}

/**
 * @brief 将目录项写回其父亲目录项的簇中
 */
static void writeBackDirent(Dirent *dirent) {
	Dirent *parentDir = dirent->parentDirent;
	fileWrite(parentDir, 0, (u64)&dirent->rawDirEnt, dirent->off, DIRENT_SIZE);
}

/**
 * @brief 创建目录
 */
int makeDirAt(struct Dirent *baseDir, char *path) {
	int r;
	char lastElem[MAX_NAME_LEN];
	Dirent *dir, *f;
	longEntSet longSet;

	if ((r = walkPath(fatFs, path, baseDir, &dir, &f, lastElem, &longSet)) == 0) {
		return -E_FILE_EXISTS;
	}

	// 出现其他错误，或者没有找到上一级的目录
	if (r != E_NOT_FOUND || dir == 0) {
		return r;
	}

	unwrap(dirAllocFile(dir, &f, lastElem));

	f->fileSize = 0;
	f->firstClus = clusterAlloc(dir->fileSystem, 0);

	writeBackDirent(f); // 写回目录项
	return 0;
}

// mount和umount系统调用由我实现，你无需实现

#define ROUNDUP(a, x) (((a) + (x)-1) & ~((x)-1))
/**
 * @brief 获取文件状态信息
 * @param kstat 内核态指针，指向文件信息结构体
 */
void fileStat(struct Dirent *file, struct kstat *pKStat) {
	memset(pKStat, 0, sizeof(pKStat));
	// P262 Linux-Unix系统编程手册
	pKStat->st_dev = file->fileSystem->deviceNumber;
	pKStat->st_ino = 0;   // 并未实现inode
	pKStat->st_mode = 0;  // 未实现
	pKStat->st_nlink = 0; // 未实现
	pKStat->st_uid = 0;
	pKStat->st_gid = 0;
	pKStat->st_rdev = 0;
	pKStat->st_size = file->fileSize;
	pKStat->st_blksize = CLUS_SIZE(file->fileSystem);
	pKStat->st_blocks = ROUNDUP(file->fileSize, pKStat->st_blksize);

	// 时间相关
	pKStat->st_atime_sec = 0;
	pKStat->st_atime_nsec = 0;
	pKStat->st_mtime_sec = 0;
	pKStat->st_mtime_nsec = 0;
	pKStat->st_ctime_sec = 0;
	pKStat->st_ctime_nsec = 0;
}

// FAT32 官方文档中的函数

//-----------------------------------------------------------------------------
// ChkSum()
// Returns an unsigned byte checksum computed on an unsigned byte
// array. The array must be 11 bytes long and is assumed to contain
// a name stored in the format of a MS-DOS directory entry.
// Passed: pFcbName Pointer to an unsigned byte array assumed to be
// 11 bytes long.
// Returns: Sum An 8-bit unsigned checksum of the array pointed
// to by pFcbName.
//------------------------------------------------------------------------------
unsigned char checkSum(unsigned char *pFcbName) {
	short FcbNameLen;
	unsigned char Sum;
	Sum = 0;
	for (FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) {
		// NOTE: The operation is an unsigned char rotate right
		Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
	}
	return (Sum);
}

char buf[8192];
void fat32Test() {
	// 测试读取文件
	Dirent *file = getFile(NULL, "/text.txt");
	assert(file != NULL);
	panic_on(fileRead(file, 0, (u64)buf, 0, file->fileSize) < 0);
	printf("%s\n", buf);

	// 测试写入文件
	char *str = "Hello! I\'m "
		    "zrp!"
		    "\n3233333333233333333233333333233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "233333333233333333222222222233233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "233333333233333333222222222233233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "233333333233333333222222222233233333333233333333233333333233333333233333333233"
		    "333333233333333233333333233333333233333333233333333233333333233333333233333333"
		    "23333333323333333322222222222222222222222222\n This is end!";
	int len = strlen(str) + 1;
	panic_on(fileWrite(file, 0, (u64)str, 0, len) < 0);

	// 读出文件
	panic_on(fileRead(file, 0, (u64)buf, 0, file->fileSize) < 0);
	printf("%s\n", buf);

	// TODO: 写一个删除文件的函数
	// 测试创建文件
	panic_on(createFile(NULL, "/zrp123456789zrp.txt", &file) < 0);
	char *str2 = "Hello! I\'m zrp!\n";
	panic_on(fileWrite(file, 0, (u64)str2, 0, strlen(str2) + 1) < 0);

	// 读取刚创建的文件
	file = getFile(NULL, "/zrp123456789zrp.txt");
	assert(file != NULL);
	panic_on(fileRead(file, 0, (u64)buf, 0, file->fileSize) < 0);
	printf("file zrp.txt: %s\n", buf);

	log(LEVEL_GLOBAL, "FAT32 Test Passed!\n");
}
