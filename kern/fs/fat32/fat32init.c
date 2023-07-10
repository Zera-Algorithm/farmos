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

FileSystem *fatFs;

// recursive
/**
 * @brief 递归建立树结构，需要保证parent的child_list已经初始化过
 */
static void build_dirent_tree(Dirent *parent) {
	Dirent *child;
	int off = 0; // 当前读到的偏移位置
	int ret;

	while (1) {
		ret = dirGetDentFrom(parent, off, &child, &off, NULL);
		if (ret == 0) {
			// 读到末尾
			break;
		}
		LIST_INSERT_HEAD(&parent->child_list, child, dirent_link);

		// 向下一层递归
		build_dirent_tree(child);
	}
}

/**
 * @brief 用fat32初始化一个文件系统，根目录记录在fs->root中
 */
void fat32_init(FileSystem *fs) {
	// 1. 以fs为单位初始化簇管理器
	log(LEVEL_GLOBAL, "fat32 is initing...\n");
	strncpy(fs->name, "FAT32", 8);
	panic_on(clusterInit(fs));

	log(LEVEL_GLOBAL, "cluster Init Finished!\n");

	// 2. 初始化根目录
	fs->root = dirent_alloc();
	strncpy(fs->root->name, "/", 2);
	fs->root->file_system = fs; // 此句必须放在countCluster之前，用于设置fs

	// 设置Dirent属性
	fs->root->first_clus = fs->superBlock.bpb.root_clus;
	log(LEVEL_GLOBAL, "first clus of root is %d\n", fs->root->first_clus);
	fs->root->raw_dirent.DIR_Attr = ATTR_DIRECTORY;
	fs->root->raw_dirent.DIR_FileSize = 0; // 目录的Dirent的size都是0
	fs->root->type = DIRENT_DIR;
	fs->root->file_size = countClusters(fs->root) * CLUS_SIZE(fs);

	// 设置树状结构
	fs->root->parent_dirent = NULL; // 父节点为空，表示已经到达根节点
	LIST_INIT(&fs->root->child_list);

	fs->root->linkcnt = 1;

	/* 不需要初始化fs->root的锁，因为在分配时即初始化了 */

	log(LEVEL_GLOBAL, "root directory init finished!\n");
	assert(sizeof(FAT32Directory) == DIRENT_SIZE);

	// 3. 递归建立Dirent树
	build_dirent_tree(fs->root);
	log(LEVEL_GLOBAL, "fat32 init finished!\n");
}

void initRootFs() {
	extern FileSystem *fatFs;
	allocFs(&fatFs);

	fatFs->image = NULL;
	fatFs->deviceNumber = 0;

	fat32_init(fatFs);
}