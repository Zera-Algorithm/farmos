#ifndef _CLUSTER_H
#define _CLUSTER_H

#include <fs/fs.h>
#include <types.h>

// FarmOS 定义的函数
err_t fatInit(FileSystem *fs);
void clusterRead(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);
void clusterWrite(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);

u64 clusterAlloc(FileSystem *fs, u64 prevCluster) __attribute__((warn_unused_result));			
void clusterFree(FileSystem *fs, u64 cluster, u64 prevCluster);

u32 fatRead(FileSystem *fs, u64 cluster);

#endif