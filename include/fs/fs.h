#ifndef _FS_H
#define _FS_H

#include <lib/queue.h>
#include <types.h>

#define MAX_NAME_LEN 100

typedef struct FileSystem FileSystem;
typedef struct Dirent Dirent;
typedef struct SuperBlock SuperBlock;

// FarmOS Dirent
struct Dirent {
	// char filename[FAT32_MAX_FILENAME + 1];
	u8 attribute; // 是文件还是目录
	// u8   create_time_tenth;
	// u16  create_time;
	// u16  create_date;
	// u16  last_access_date;
	u32 first_clus;
	// u16  last_write_time;
	// u16  last_write_date;
	u32 file_size;

	u32 cur_clus;
	u32 inodeMaxCluster;
	u32 clus_cnt;
	// Inode inode;

	u8 _nt_res;
	FileSystem *fileSystem;
	/* for OS */
	// 操作系统相关的数据结构
	enum { ZERO = 10, OSRELEASE = 12, NONE = 15 } dev;
	FileSystem *head;
	u32 off;	// offset in the parent dir entry, for writing convenience
	Dirent *parent; // because FAT32 doesn't have such thing like inum,
	Dirent *nextBrother;
	Dirent *firstChild;
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
	char name[MAX_NAME_LEN];
	SuperBlock superBlock;					    // 超级块
	Dirent root;						    // root项
	struct File *image;					    // mount对应的文件
	LIST_ENTRY(FileSystem) fsLink;				    // 串接项
	int deviceNumber;					    // 对应真实设备的编号
	struct Buffer *(*get)(struct FileSystem *fs, u64 blockNum); // 读取FS的一个Buffer
	// 强制规定：传入的fs即为本身的fs
	// 稍后用read返回的这个Buffer指针进行写入和释放动作
	// 我们默认所有文件系统（不管是挂载的，还是从virtio读取的），都需要经过缓存层
};

#endif
