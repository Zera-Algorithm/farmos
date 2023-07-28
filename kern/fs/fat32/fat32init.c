#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/dirent.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <fs/initcall.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/wchar.h>
#include <lock/mutex.h>

FileSystem *fatFs;
extern mutex_t mtx_file;

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
		// printf("get child: %s, parent: %s\n", child->name, parent->name);

		// 跳过.和..
		if (strncmp(child->name, ".          ", 11) == 0 ||
		    strncmp(child->name, "..         ", 11) == 0) {
			continue;
		}
		LIST_INSERT_HEAD(&parent->child_list, child, dirent_link);

		// 如果为目录，就向下一层递归
		if (child->type == DIRENT_DIR) {
			build_dirent_tree(child);
		}
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
	extern struct FileDev file_dev_file;
	fs->root->dev = &file_dev_file;

	// 设置树状结构
	fs->root->parent_dirent = NULL; // 父节点为空，表示已经到达根节点
	LIST_INIT(&fs->root->child_list);

	fs->root->linkcnt = 1;

	/* 不需要初始化fs->root的锁，因为在分配时即初始化了 */

	log(LEVEL_GLOBAL, "root directory init finished!\n");
	assert(sizeof(FAT32Directory) == DIRENT_SIZE);

	// 3. 递归建立Dirent树
	build_dirent_tree(fs->root);
	log(LEVEL_GLOBAL, "build dirent tree succeed!\n");
	log(LEVEL_GLOBAL, "fat32 init finished!\n");
}

void init_root_fs() {
	extern FileSystem *fatFs;
	extern mutex_t mtx_fs;
	mtx_init(&mtx_fs, "fs", false, MTX_SPIN);
	mtx_init(&mtx_file, "mtx_file", true, MTX_SLEEP | MTX_RECURSE);

	allocFs(&fatFs);

	fatFs->image = NULL;
	fatFs->deviceNumber = 0;

	fat32_init(fatFs);
}

static void init_dev_fs() {
	makeDirAt(fatFs->root, "/dev", 0);

	// 这两个暂时用空文件代替
	panic_on(create_file_and_close("/dev/random"));
	panic_on(create_file_and_close("/dev/rtc"));
	panic_on(create_file_and_close("/dev/rtc0"));
	makeDirAt(fatFs->root, "/dev/misc", 0);
	panic_on(create_file_and_close("/dev/misc/rtc"));

	extern struct FileDev file_dev_null;
	extern struct FileDev file_dev_zero;

	Dirent *file1, *file2;
	panic_on(createFile(fatFs->root, "/dev/null", &file1));
	panic_on(createFile(fatFs->root, "/dev/zero", &file2));

	file1->dev = &file_dev_null;
	file2->dev = &file_dev_zero;
	file1->type = DIRENT_CHARDEV;
	file2->type = DIRENT_CHARDEV;

	file_close(file1);
	file_close(file2);
}

static void init_proc_fs() {
	makeDirAt(fatFs->root, "/proc", 0);

	extern initcall_t __initcall_fs_start[], __initcall_fs_end[];
	initcall_t *fn;

	int count = __initcall_fs_end - __initcall_fs_start;
	log(LEVEL_GLOBAL, "initcall count: %d\n", count);

	for (fn = __initcall_fs_start; fn < __initcall_fs_end; fn++) {
		log(LEVEL_GLOBAL, "executing initcall #%d\n", fn - __initcall_fs_start);
		(*fn)();
	}
}

static void init_fs_other() {
	makeDirAt(fatFs->root, "/bin", 0);
	panic_on(create_file_and_close("/bin/ls"));

	makeDirAt(fatFs->root, "/etc", 0);
	makeDirAt(fatFs->root, "/tmp", 0);

	// 将默认的动态链接库链接到/lib目录
	makeDirAt(fatFs->root, "/lib", 0);
	panic_on(linkat(fatFs->root, "/libc.so", fatFs->root, "/lib/ld-musl-riscv64-sf.so.1"));
	panic_on(linkat(fatFs->root, "/tls_get_new-dtv_dso.so", fatFs->root, "/lib/tls_get_new-dtv_dso.so"));
}

void init_files() {
	init_dev_fs();
	init_proc_fs();
	init_fs_other();
}
