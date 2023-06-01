#ifndef _DIRENT_H
#define _DIRENT_H
#include <fs/fat32.h>
#include <fs/fs.h>

void direntInit();
Dirent *direntAlloc();
void direntDeAlloc(Dirent *dirent);

#endif
