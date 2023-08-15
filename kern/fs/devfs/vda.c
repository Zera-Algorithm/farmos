#include <fs/dirent.h>
#include <fs/file_device.h>
#include <fs/vfs.h>
#include <fs/buf.h>
#include <lib/log.h>
#include <lib/transfer.h>

static int dev = 0;

static int vda_read(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	u64 begin = off, end = off + n;
	Buffer *buf;
	if (begin % BUF_SIZE != 0) {
		buf = bufRead(dev, begin / BUF_SIZE, true);
		copyOut(dst, buf->data->data + begin % BUF_SIZE, MIN(BUF_SIZE - begin % BUF_SIZE, n));
		bufRelease(buf);
	}
	begin = ROUNDUP(begin, BUF_SIZE);
	while (begin < end) {
		buf = bufRead(dev, begin / BUF_SIZE, true);
		copyOut(dst + begin - off, buf->data->data, MIN(BUF_SIZE, end - begin));
		bufRelease(buf);
		begin += BUF_SIZE;
	}
	return n;
}

/**
 * @brief vda可以无限写入
 */
static int vda_write(struct Dirent *file, int user, u64 src, uint off, uint n) {
	u64 begin = off, end = off + n;
	Buffer *buf;
	if (begin % BUF_SIZE != 0) {
		buf = bufRead(dev, begin / BUF_SIZE, true);
		copyIn(src, buf->data->data + begin % BUF_SIZE, MIN(BUF_SIZE - begin % BUF_SIZE, n));
		bufWrite(buf);
		bufRelease(buf);
	}
	begin = ROUNDUP(begin, BUF_SIZE);
	while (begin < end) {
		buf = bufRead(dev, begin / BUF_SIZE, true);
		copyIn(src + begin - off, buf->data->data, MIN(BUF_SIZE, end - begin));
		bufWrite(buf);
		bufRelease(buf);
		begin += BUF_SIZE;
	}
	return n;
}

/**
 * vda文件设备
 */

struct FileDev file_dev_vda = {
    .dev_id = 'f',
    .dev_name = "vda_file",
    .dev_read = vda_read,
    .dev_write = vda_write,
};
