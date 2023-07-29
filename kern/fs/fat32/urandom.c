#include <fs/dirent.h>
#include <fs/file_device.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/transfer.h>

static int urandom_read(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	char ch = 'a' + (off * off % 26); // 尽量随机
	for (int i = 0; i < n; i++) {
		copyOut(dst + i, &ch, 1);
	}
	return n;
}

/**
 * @brief urandom可以无限写入
 */
static int urandom_write(struct Dirent *file, int user, u64 src, uint off, uint n) {
	return n;
}

/**
 * urandom文件设备
 */

struct FileDev file_dev_urandom = {
    .dev_id = 'f',
    .dev_name = "urandom_file",
    .dev_read = urandom_read,
    .dev_write = urandom_write,
};
