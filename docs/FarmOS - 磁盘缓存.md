# FarmOS 缓冲区

## 概述

缓冲区层为磁盘 IO 提供了缓存，建立在磁盘驱动层之上，为上层屏蔽了硬件操作，提供了统一的磁盘访问接口，目前采用 Write-Through 策略，写操作时直接同步回磁盘。

缓冲区使用缓冲区链表维护，为提高并发性能，使用缓冲区时按照磁盘块序号对缓冲区进行分组，每个缓冲区组有一个缓冲区组锁，缓冲区组锁保护缓冲区组的缓冲区链表。

缓冲区使用 LRU 算法进行缓冲区替换，每次一个缓冲区使用结束时将其更新到链表头部。当没有空闲的缓冲区控制块时，释放当前最接近链表尾部且不在使用的缓冲区控制块。

## 缓冲区结构

每个缓存块都有一个缓冲区控制块，用于描述缓存块的属性。缓冲区控制块的定义如下：

```c
typedef struct BufferData {
	u8 data[BUF_SIZE];
} BufferData;

typedef struct Buffer {
	// 缓冲区控制块属性
	u64 blockno;
	i32 dev;
	bool valid;
	u16 disk;
	u16 refcnt;
	BufferData *data;
	struct sleeplock lock;
	TAILQ_ENTRY(Buffer) link;
} Buffer;
```

若干个缓冲区被组成一个缓冲区组，缓冲区组的定义如下：

```c

typedef struct BufferGroup {
	BufList list; // 缓冲区双向链表（越靠前使用越频繁）
	Buffer buf[BGROUP_BUF_NUM];
	struct spinlock lock;
} BufferGroup;

```

## 缓冲区接口

缓冲区为上层提供了三个接口：

- `bufRead`：从磁盘读取一个缓冲区，并对缓冲区数据加锁
- `bufWrite`：对于给定的当前进程持有锁的缓冲区，将缓冲区写入磁盘
- `bufRelease`：对于给定的当前进程持有锁的缓冲区，释放缓冲区
