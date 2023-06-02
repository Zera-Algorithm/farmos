#ifndef _VFS_H
#define _VFS_H

#include <fs/fs.h>
#include <types.h>

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

void fileGetPath(Dirent *dirent, char *path);

#endif
