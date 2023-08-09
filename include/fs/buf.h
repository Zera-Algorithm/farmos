#ifndef _BUF_H
#define _BUF_H

#include <lib/queue.h>
#include <lock/sleeplock.h>
#include <mm/memlayout.h>
#include <types.h>
#include <param.h>

#ifdef FEATURE_LESS_MEMORY
#define BUF_SUM_SIZE (32 * 1024 * 1024) // 32MB
#else
#define BUF_SUM_SIZE (64 * 1024 * 1024) // 64MB
#endif


#define BUF_SIZE (512)			// 512B
#define BGROUP_NUM (1 << 13)		// 1024 * 8

#define BGROUP_MASK (BGROUP_NUM - 1)			      // 0x3ff
#define BGROUP_BUF_NUM (BUF_SUM_SIZE / BGROUP_NUM / BUF_SIZE) // 64
#define BUF_NUM (BUF_SUM_SIZE / BUF_SIZE)

typedef struct BufferData {
	u8 data[BUF_SIZE];
} BufferData;

typedef struct BufferDataGroup {
	BufferData buf[BGROUP_BUF_NUM];
} BufferDataGroup;

typedef struct Buffer {
	// 缓冲区控制块属性
	u64 blockno;
	i32 dev;
	bool valid;
	bool dirty;
	u16 disk;
	u16 refcnt;
	BufferData *data;
	struct sleeplock lock;
	TAILQ_ENTRY(Buffer) link;
} Buffer;

typedef TAILQ_HEAD(BufList, Buffer) BufList;

typedef struct BufferGroup {
	BufList list; // 缓冲区双向链表（越靠前使用越频繁）
	Buffer buf[BGROUP_BUF_NUM];
	struct spinlock lock;
} BufferGroup;

void bufInit();
void bufTest(u64 blockno);

Buffer *bufRead(u32 dev, u64 blockno) __attribute__((warn_unused_result));
void bufWrite(Buffer *buf);
void bufRelease(Buffer *buf);
void bufSync();

// struct buf {
// 	int valid; // has data been read from disk?
// 	int disk;  // does disk "own" buf?
// 	u32 dev;
// 	u32 blockno;
// 	struct sleeplock lock;
// 	u32 refcnt;
// 	struct buf *prev; // LRU cache list
// 	struct buf *next;
// 	uchar data[BSIZE];
// };

#endif
