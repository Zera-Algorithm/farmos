#ifndef _FS_H
#define _FS_H

#include <fs/fat32.h>
#include <lib/queue.h>
#include <types.h>

#define MAX_NAME_LEN 256

typedef struct FileSystem FileSystem;
typedef struct Dirent Dirent;
typedef struct SuperBlock SuperBlock;

LIST_HEAD(DirentList, Dirent);

// FarmOS Dirent
struct Dirent {
	FAT32Directory rawDirEnt; // 原生的dirent项
	char name[MAX_NAME_LEN];

	// 所在的文件系统
	FileSystem *fileSystem;
	u32 firstClus; // 第一个簇的簇号
	u64 fileSize;  // 文件大小
	/* for OS */
	// 操作系统相关的数据结构
	enum { ZERO = 10, OSRELEASE = 12, NONE = 15 } dev;
	// FileSystem *head;
	u32 off; // 在上一个目录项中的内容偏移，用于写回

	LIST_ENTRY(Dirent) direntLink; // 自己的链接
	struct DirentList childList;   // 子Dirent列表
	struct Dirent *parentDirent;   // 父亲Dirent列表
};

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
	SuperBlock superBlock; // 超级块
	Dirent root;	       // root项
	struct Fd *image;      // mount对应的文件描述符
	int deviceNumber;      // 对应真实设备的编号，暂不使用
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

void allocFs(struct FileSystem **pFs);
void deAllocFs(struct FileSystem *fs);

#endif
