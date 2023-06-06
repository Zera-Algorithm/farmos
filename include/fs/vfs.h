#ifndef _VFS_H
#define _VFS_H

#include <fs/fs.h>
#include <types.h>

// 文件系统层接口函数
struct Dirent *getFile(struct Dirent *baseDir, char *path);
int createFile(struct Dirent *baseDir, char *path, Dirent **file);
int fileRead(struct Dirent *file, int user, u64 dst, uint off, uint n);
int fileWrite(struct Dirent *file, int user, u64 src, uint off, uint n);
int linkAt(struct Dirent *oldDir, char *oldPath, struct Dirent *newDir, char *newPath);
int unLinkAt(struct Dirent *dir, char *path);
int makeDirAt(Dirent *baseDir, char *path, int mode);
void fileStat(struct Dirent *file, struct kstat *pKStat);

int dirGetDentFrom(Dirent *dir, u64 offset, struct Dirent **file, int *next_offset,
		   longEntSet *longSet);
void fileGetPath(Dirent *dirent, char *path);
void initRootFs();

int mountFs(char *special, Dirent *baseDir, char *dirPath);
int umountFs(char *dirPath, Dirent *baseDir);

#endif
