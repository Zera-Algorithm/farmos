#ifndef _BUF_H
#define _BUF_H
#include <fs/fs.h>
#include <lock/sleeplock.h>
#include <lock/spinlock.h>
#include <types.h>

struct buf {
	int valid; // has data been read from disk?
	int disk;  // does disk "own" buf?
	u32 dev;
	u32 blockno;
	struct sleeplock lock;
	u32 refcnt;
	struct buf *prev; // LRU cache list
	struct buf *next;
	uchar data[BSIZE];
};
#endif
