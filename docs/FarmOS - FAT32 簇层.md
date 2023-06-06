# FarmOS FAT32 文件系统簇层

## 概述

FAT32 文件系统簇层的主要功能是管理簇，并向 FAT32 文件系统的上层屏蔽对缓冲区的操作，提供统一的簇读写接口。

上层文件层仅通过簇层接口访问文件系统中的簇，而不直接访问缓冲区。

## 簇层接口

簇层为文件层提供了簇的读写、簇的分配与释放、FAT 表项的读取等接口。

簇层接口函数声明如下：
    
```c
void clusterRead(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);
void clusterWrite(FileSystem *fs, u64 cluster, off_t offset, void *dst, size_t n, bool isUser);

u64 clusterAlloc(FileSystem *fs, u64 prevCluster) __attribute__((warn_unused_result));
void clusterFree(FileSystem *fs, u64 cluster, u64 prevCluster);

u32 fatRead(FileSystem *fs, u64 cluster);
i64 fileBlockNo(FileSystem *fs, u64 firstclus, u64 fblockno);
```

接口函数意义如下：
- `clusterRead`：在给定的文件系统中读取对应簇的数据
- `clusterWrite`：在给定的文件系统中写入对应簇的数据
- `clusterAlloc`：在给定的文件系统中分配一个空闲簇
- `clusterFree`：在给定的文件系统中释放一个簇
- `fatRead`：在给定的文件系统中读取对应簇的 FAT 表项
- `fileBlockNo`：在给定的文件系统中计算文件中给定文件块号对应的簇号，一般用于 `mount`。