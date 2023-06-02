#ifndef _CLUSTER_H
#define _CLUSTER_H

#include <fs/fs.h>
#include <types.h>

// 7个f，最高4位保留
#define FAT32_EOF 0xffffffful
#define FAT32_NOT_END_CLUSTER(cluster) ((cluster) < 0x0ffffff8ul)

// FarmOS 定义的函数
err_t clusterInit(FileSystem *fs);
void clusterRead(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);
void clusterWrite(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);

u64 clusterAlloc(FileSystem *fs, u64 prevCluster) __attribute__((warn_unused_result));
void clusterFree(FileSystem *fs, u64 cluster, u64 prevCluster);

u32 fatRead(FileSystem *fs, u64 cluster);

#endif
