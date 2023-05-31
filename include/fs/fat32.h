#ifndef _FAT32_H
#define _FAT32_H

#include <types.h>

typedef struct FAT32BootParamBlock {
	// 通用的引导扇区属性
	u8 BS_jmpBoot[3];
	u8 BS_OEMName[8];
	u16 BPB_BytsPerSec;
	u8 BPB_SecPerClus;
	u16 BPB_RsvdSecCnt;
	u8 BPB_NumFATs;
	u16 BPB_RootEntCnt;
	u16 BPB_TotSec16;
	u8 BPB_Media;
	u16 BPB_FATSz16;
	u16 BPB_SecPerTrk;
	u16 BPB_NumHeads;
	u32 BPB_HiddSec;
	u32 BPB_TotSec32;
	// FAT32 引导扇区属性
	u32 BPB_FATSz32;
	u16 BPB_ExtFlags;
	u16 BPB_FSVer;
	u32 BPB_RootClus;
	u16 BPB_FSInfo;
	u16 BPB_BkBootSec;
	u8 BPB_Reserved[12];
	u8 BS_DrvNum;
	u8 BS_Reserved1;
	u8 BS_BootSig;
	u32 BS_VolID;
	u8 BS_VolLab[11];
	u8 BS_FilSysType[8];
	u8 BS_CodeReserved[420];
	u8 BS_Signature[2];
} __attribute__((packed)) FAT32BootParamBlock;

typedef struct FAT32Directory {
	u8 DIR_Name[11];
	u8 DIR_Attr;
	u8 DIR_NTRes;
	u8 DIR_CrtTimeTenth;
	u16 DIR_CrtTime;
	u16 DIR_CrtDate;
	u16 DIR_LstAccDate;
	u16 DIR_FstClusHI;
	u16 DIR_WrtTime;
	u16 DIR_WrtDate;
	u16 DIR_FstClusLO;
	u32 DIR_FileSize;
} __attribute__((packed)) FAT32Directory;

// 长文件名项存储文件名的wchar版本，所以读取文件名只能通过长文件名项
typedef struct FAT32LongDirectory {
	u8 LDIR_Ord;
	wchar LDIR_Name1[5];
	u8 LDIR_Attr;
	u8 LDIR_Type;
	u8 LDIR_Chksum;
	wchar LDIR_Name2[6];
	u16 LDIR_FstClusLO;
	wchar LDIR_Name3[2];
} __attribute__((packed)) FAT32LongDirectory;

#define BPB_SIZE sizeof(FAT32BootParamBlock)
#define DIR_SIZE sizeof(FAT32Directory)

// FAT32 文件、目录属性
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME_MASK (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define CHAR2LONGENT    26
#define LAST_LONG_ENTRY     0x40


// FarmOS自定义属性，用来表示链接文件
#define ATTR_LINK 0x40

// FAT32 结构体大小定义
#define DIRENT_SIZE 32

unsigned char checkSum(unsigned char *pFcbName);
void fat32Init();
void fat32Test();

// 文件系统层接口函数
struct Dirent *getFile(struct Dirent *baseDir, char *path);
int createFile(struct Dirent *baseDir, char *path, Dirent **file);
int fileRead(struct Dirent *file, int user, u64 dst, uint off, uint n);
int fileWrite(struct Dirent *file, int user, u64 src, uint off, uint n);
int getDents(struct Dirent *dir, struct DirentUser *buf, int len);
int linkAt(struct Dirent *oldDir, char *oldPath, struct Dirent *newDir, char *newPath);
int unLinkAt(struct Dirent *dir, char *path);
int makeDirAt(struct Dirent *baseDir, char *path);
void fileStat(struct Dirent *file, struct kstat *pKStat);


#endif
