#include <dev/virtio.h>
#include <fs/buf.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>
#include <mm/memlayout.h>

BufferDataGroup *bufferData;
BufferGroup *bufferGroups;

void bufInit() {
	log(MM_GLOBAL, "bufInit\n");
	for (int i = 0; i < BGROUP_NUM; i++) {
		// 初始化缓冲区组
		BufferDataGroup *bdata = &bufferData[i];
		BufferGroup *b = &bufferGroups[i];
		// todo: Init Lock
		TAILQ_INIT(&b->list);
		for (int j = 0; j < BGROUP_BUF_NUM; j++) {
			// 初始化第 i 组的缓冲区
			Buffer *buf = &b->buf[j];
			buf->dev = -1;
			buf->data = &bdata->buf[j];
			// TODO: Init Lock
			TAILQ_INSERT_TAIL(&b->list, buf, link);
		}
	}
}

static Buffer *bufAlloc(u32 dev, u64 blockno) {
	u64 group = blockno & BGROUP_MASK;

	// 检查对应块是否已经被缓存
	Buffer *buf;
	TAILQ_FOREACH (buf, &bufferGroups[group].list, link) {
		// 如果已经被缓存，直接返回 todo:lock
		if (buf->dev == dev && buf->blockno == blockno) {
			buf->refcnt++;
			log(BUF_MODULE, "BufAlloc HIT: <dev: %d, blockno: %d> in Buffer[%d][%d]\n",
			    dev, blockno, group, buf - bufferGroups[group].buf);
			return buf;
		}
	}

	// 没有被缓存，找到最久未使用的缓冲区（LRU策略换出）
	TAILQ_FOREACH_REVERSE(buf, &bufferGroups[group].list, BufList, link) {
		if (buf->refcnt == 0) {
			if (buf->valid && buf->dirty) {
				// 如果该缓冲区已经被使用，写回磁盘
				// 即换出时写回磁盘
				virtio_disk_rw(buf, 1);
			}

			// 如果该缓冲区没有被引用，直接使用
			buf->dev = dev;
			buf->blockno = blockno;
			buf->valid = 0;
			buf->dirty = 0;
			buf->refcnt = 1;
			log(BUF_MODULE, "BufAlloc MISS: <dev: %d, blockno: %d> in Buffer[%d][%d]\n",
			    dev, blockno, group, buf - bufferGroups[group].buf);
			return buf;
		}
	}

	error("No Buffer Available!\n");
}

Buffer *bufRead(u32 dev, u64 blockno) {
	Buffer *buf = bufAlloc(dev, blockno);
	if (!buf->valid) {
		virtio_disk_rw(buf, 0);
		buf->valid = true;
	}
	return buf;
}

void bufWrite(Buffer *buf) {
	buf->dirty = true;
	return;
}

void bufRelease(Buffer *buf) {
	buf->refcnt--;
	if (buf->refcnt == 0) {
		// 刚刚完成使用的缓冲区，放在链表头部晚些被替换
		u64 group = buf->blockno & BGROUP_MASK;
		TAILQ_REMOVE(&bufferGroups[group].list, buf, link);
		TAILQ_INSERT_HEAD(&bufferGroups[group].list, buf, link);
	}
}

void bufTest(u64 blockno) {
	log(LEVEL_GLOBAL, "begin buf test!\n");

	// 测试写入0号扇区（块）
	Buffer *b0 = bufRead(0, blockno);
	for (int i = 0; i < BUF_SIZE; i++) {
		b0->data->data[i] = (u8)(blockno % 0xff) + i % 10;
	}
	b0->data->data[BUF_SIZE - 1] = 0;
	bufWrite(b0);
	Buffer b0_copy = *b0;
	bufRelease(b0);

	// 测试读出0号扇区
	b0 = bufRead(0, blockno);
	assert(strncmp((const char *)b0->data, (const char *)b0_copy.data, BUF_SIZE) == 0);
	bufRelease(b0);

	log(LEVEL_GLOBAL, "buf test %d passed!\n", blockno);
}
