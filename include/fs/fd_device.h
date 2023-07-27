#ifndef _DEVICE_H
#define _DEVICE_H

#include <types.h>

// 基于内核fd的读写，需要自行避免并发
// fd的offset由本层维护
// 其中的buf均是用户态地址
typedef struct FdDev {
	int dev_id;
	char *dev_name; // 设备名
	int (*dev_read)(struct Fd *fd, u64 buf, u64 n, u64 offset);
	int (*dev_write)(struct Fd *fd, u64 buf, u64 n, u64 offset);
	int (*dev_close)(struct Fd *fd);
	int (*dev_stat)(struct Fd *fd, u64 pkStat);
} FdDev;

#endif
