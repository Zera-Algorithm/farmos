#ifndef _FD_H
#define _FD_H

#include <fs/fat32.h>
#include <fs/fs.h>
#include <types.h>

#define FDNUM 1024

struct Fd {
	Dirent *dirent;
	// PIPE
	int type;
	uint offset;
	uint flags;
	struct kstat stat;
};

typedef struct DirentUser {
	uint64 d_ino;		 // 索引结点号
	i64 d_off;		 // 下一个dirent到文件首部的偏移
	unsigned short d_reclen; // 当前dirent的长度
	unsigned char d_type;	 // 文件类型
	char d_name[];		 //文件名
} DirentUser;

#define DIRENT_USER_SIZE 32
#define DIRENT_USER_OFFSET_NAME ((u64) & (((DirentUser *)0)->d_name))
#define DIRENT_NAME_LENGTH (DIRENT_USER_SIZE - DIRENT_USER_OFFSET_NAME)

extern struct Fd fds[FDNUM];
extern uint citesNum[FDNUM];

#define dev_file 1
#define dev_pipe 2
#define dev_console 3

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_ACCMODE 0x003
#define O_CREATE 0x40
// TODO CREATE标志位存疑
#define O_TRUNC 0x400

int fdAlloc();
int closeFd(int fd);
void cloneAddCite(uint i);
int read(int fd, u64 buf, size_t count);
int write(int fd, u64 buf, size_t count);
int readConsoleAlloc();
int writeConsoleAlloc();
int errorConsoleAlloc();
int dup(int fd);
int dup3(int old, int new);
void freeFd(uint i);
int getdents64(int fd, u64 buf, int len);

#endif
