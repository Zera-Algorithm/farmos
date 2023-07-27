#ifndef _FS_H
#define _FS_H

#include <fs/fat32.h>
#include <fs/file_time.h>
#include <lib/queue.h>
#include <mm/memlayout.h>
#include <types.h>

#define MAX_NAME_LEN 256

typedef struct FileSystem FileSystem;
typedef struct Dirent Dirent;
typedef struct SuperBlock SuperBlock;

LIST_HEAD(DirentList, Dirent);

// 对应目录、文件、设备
typedef enum dirent_type { DIRENT_DIR, DIRENT_FILE, DIRENT_CHARDEV, DIRENT_BLKDEV } dirent_type_t;

struct TwicePointer {
	u32 cluster[PAGE_SIZE / sizeof(u32)];
};

struct ThirdPointer {
	struct TwicePointer *ptr[PAGE_SIZE / sizeof(struct TwicePointer *)];
};

// 指向簇列表的指针
typedef struct DirentPointer {
	// 一级指针
	u32 first[10];
	struct TwicePointer *second[10];
	struct ThirdPointer *third;
} DirentPointer;

// 用于调试Dirent引用计数次数的开关
// #define REFCNT_DEBUG

struct file_time;

// FarmOS Dirent
struct Dirent {
	FAT32Directory raw_dirent; // 原生的dirent项
	char name[MAX_NAME_LEN];

	// 文件系统相关属性
	FileSystem *file_system; // 所在的文件系统
	u32 first_clus;		 // 第一个簇的簇号
	u64 file_size;		 // 文件大小

	/* for OS */
	// 操作系统相关的数据结构
	// 仅用于是挂载点的目录，指向该挂载点所对应的文件系统。用于区分mount目录和非mount目录
	FileSystem *head;

	DirentPointer pointer;

	// [暂不用] 标记此Dirent节点是否已扩展子节点，用于弹性伸缩Dirent缓存，不过一般设置此字段为1
	// 我们会在初始化时扫描所有文件，并构建Dirent
	u32 is_extend;

	// 在上一个目录项中的内容偏移，用于写回
	u32 parent_dir_off;

	// 标记是文件、目录还是设备文件（仅在文件系统中出现，不出现在磁盘中）
	u32 type;

	// 文件的时间戳
	struct file_time time;

	// 设备结构体，可以通过该结构体完成对文件的读写
	struct FileDev *dev;

	// 子Dirent列表
	struct DirentList child_list;

	// 用于空闲链表和父子连接中的链接，因为一个Dirent不是在空闲链表中就是在树上
	LIST_ENTRY(Dirent) dirent_link;

	// 父亲Dirent
	struct Dirent *
	    parent_dirent; // 即使是mount的目录，也指向其上一级目录。如果该字段为NULL，表示为总的根目录

	// 各种计数
	u32 linkcnt; // 链接计数
	u32 refcnt;  // 引用计数

	char *holders[64];
	int holder_cnt;
};

// 有当前dirent引用时不变的项（只读，无需加锁）：parent_dirent, name, file_system,

#define MAX_LONGENT 8

// 用于在查询文件名时存放长文件名项
typedef struct longEntSet {
	FAT32LongDirectory *longEnt[MAX_LONGENT];
	int cnt;
} longEntSet;

// FarmOS SuperBlock
struct SuperBlock {
	u32 first_data_sec;
	u32 data_sec_cnt;
	u32 data_clus_cnt;
	u32 bytes_per_clus;

	struct {
		u16 bytes_per_sec;
		u8 sec_per_clus;
		u16 rsvd_sec_cnt;
		u8 fat_cnt;   /* count of FAT regions */
		u32 hidd_sec; /* count of hidden sectors */
		u32 tot_sec;  /* total count of sectors including all regions */
		u32 fat_sz;   /* count of sectors for a FAT region */
		u32 root_clus;
	} bpb;
};

// FarmOS VFS
struct FileSystem {
	bool valid; // 是否有效
	char name[8];
	SuperBlock superBlock;					    // 超级块
	Dirent *root;						    // root项
	struct Dirent *image;					    // mount对应的文件描述符
	struct Dirent *mountPoint;				    // 挂载点
	int deviceNumber;					    // 对应真实设备的编号
	struct Buffer *(*get)(struct FileSystem *fs, u64 blockNum); // 读取FS的一个Buffer
	// 强制规定：传入的fs即为本身的fs
	// 稍后用read返回的这个Buffer指针进行写入和释放动作
	// 我们默认所有文件系统（不管是挂载的，还是从virtio读取的），都需要经过缓存层
};

union st_mode {
	u32 val;
	// 从低地址到高地址
	struct {
		unsigned other_x : 1;
		unsigned other_w : 1;
		unsigned other_r : 1;
		unsigned group_x : 1;
		unsigned group_w : 1;
		unsigned group_r : 1;
		unsigned user_x : 1;
		unsigned user_w : 1;
		unsigned user_r : 1;
		unsigned t : 1;
		unsigned g : 1;
		unsigned u : 1;
		unsigned file_type : 4;
	} __attribute__((packed)) bits; // 取消优化对齐
};

typedef int (*findfs_callback_t)(FileSystem *fs, void *data);

void allocFs(struct FileSystem **pFs);
void deAllocFs(struct FileSystem *fs);
FileSystem *find_fs_by(findfs_callback_t findfs, void *data);

#define MAX_FS_COUNT 16

#endif
