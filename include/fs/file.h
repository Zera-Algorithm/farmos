#ifndef _FILE_H
#define _FILE_H

#include <fs/fat32.h>
#include <types.h>

#define NAME_MAX_LEN 256

int openat(int fd, u64 filename, int flags, mode_t mode);

#endif
