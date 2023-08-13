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
#include <fs/chardev.h>
#include <fs/filepnt.h>
#include <mm/kmalloc.h>

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

	filepnt_init(parent);
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

static void create_bind_device(const char *path, struct FileDev *dev, u16 type) {
	Dirent *file1;
	createFile(fatFs->root, (char *)path, &file1);
	file1->dev = dev;
	file1->type = type;
	file_close(file1);
}

static void init_dev_fs() {
	makeDirAt(fatFs->root, "/dev", 0);

	// 这两个暂时用空文件代替
	create_file_and_close("/dev/random");
	create_file_and_close("/dev/rtc");
	create_file_and_close("/dev/rtc0");
	makeDirAt(fatFs->root, "/dev/misc", 0);
	create_file_and_close("/dev/misc/rtc");
	makeDirAt(fatFs->root, "/dev/shm", 0);

	extern struct FileDev file_dev_null;
	extern struct FileDev file_dev_zero;
	extern struct FileDev file_dev_urandom;
	extern struct FileDev file_dev_tty;
	extern struct FileDev file_dev_vda;

	create_bind_device("/dev/null", &file_dev_null, DIRENT_CHARDEV);
	create_bind_device("/dev/zero", &file_dev_zero, DIRENT_CHARDEV);
	create_bind_device("/dev/urandom", &file_dev_urandom, DIRENT_CHARDEV);
	create_bind_device("/dev/tty", &file_dev_tty, DIRENT_CHARDEV);
	create_bind_device("/dev/vda", &file_dev_vda, DIRENT_BLKDEV);
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

	// 设置系统发行版本，越大越好，过小会FATAL
	makeDirAt(fatFs->root, "/proc/sys", 0);
	create_file_and_close("/proc/filesystems");
	makeDirAt(fatFs->root, "/proc/sys/kernel", 0);
	create_chardev_file("/proc/sys/kernel/osrelease", "10.2.0", NULL, NULL);
}

static void create_file_and_write(const char *path, void *content, int size) {
	Dirent *file1;
	if (getFile(fatFs->root, (char *)path, &file1) < 0) {
		createFile(fatFs->root, (char *)path, &file1);
	}
	if (file1 == NULL) {
		warn("file %s get and create err!\n", path);
	}
	file_write(file1, 0, (u64)content, 0, size);
	file_close(file1);
}

static void init_fs_other() {
	create_file_and_close("/bin/ls");

	makeDirAt(fatFs->root, "/etc", 0);
	makeDirAt(fatFs->root, "/tmp", 0);

	create_file_and_close("/etc/filesystems");

	// 将默认的动态链接库链接到/lib目录
	makeDirAt(fatFs->root, "/lib", 0);
	linkat(fatFs->root, "/libc.so", fatFs->root, "/lib/ld-musl-riscv64-sf.so.1");
	linkat(fatFs->root, "/tls_get_new-dtv_dso.so", fatFs->root, "/lib/tls_get_new-dtv_dso.so");
	linkat(fatFs->root, "/busybox", fatFs->root, "/bin/busybox");
	linkat(fatFs->root, "/iozone", fatFs->root, "/bin/iozone");


	makeDirAt(fatFs->root, "/sbin", 0);
	linkat(fatFs->root, "/lmbench_all", fatFs->root, "/sbin/lmbench_all");
	linkat(fatFs->root, "/busybox", fatFs->root, "/sbin/busybox");
	linkat(fatFs->root, "/lua", fatFs->root, "/sbin/lua");

	// 两个部分的sh脚本
	extern const unsigned char bin2c_iperf_testcode_sh[637];
	extern const unsigned char bin2c_unixbench_testcode_sh[4556];
	extern const unsigned char bin2c_sort_src[8546];
	extern const unsigned char bin2c_lmbench_testcode_sh[1170];
	extern const unsigned char binary_unixbench_looper[491];
	create_chardev_file("/iperf_testcode_part.sh", (char *)bin2c_iperf_testcode_sh, NULL, NULL);
	create_chardev_file("/unixbench_testcode_part.sh", (char *)bin2c_unixbench_testcode_sh, NULL, NULL);
	create_chardev_file("/unixbench_looper.sh", (char *)binary_unixbench_looper, NULL, NULL);

	// 写入一个空格文件
	char *cont = kmalloc(PAGE_SIZE);
	memset(cont, ' ', PAGE_SIZE);
	create_file_and_write("/lat_sig", cont, PAGE_SIZE);
	kfree(cont);

	create_file_and_write("sort.src", (void *)bin2c_sort_src, sizeof(bin2c_sort_src));
	create_file_and_write("/lmbench_testcode_part.sh", (void *)bin2c_lmbench_testcode_sh, sizeof(bin2c_lmbench_testcode_sh));
}

void init_files() {
	init_dev_fs();
	init_proc_fs();
	init_fs_other();
}
