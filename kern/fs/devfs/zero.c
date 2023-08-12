#include <fs/dirent.h>
#include <fs/file_device.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/transfer.h>

static int zero_read(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	char ch = 0;
	for (int i = 0; i < n; i++) {
		copyOut(dst + i, &ch, 1);
	}
	return n;
}

/**
 * @brief zero可以无限写入
 */
static int zero_write(struct Dirent *file, int user, u64 src, uint off, uint n) {
	return n;
}

/**
 * zero文件设备
 */

struct FileDev file_dev_zero = {
    .dev_id = 'f',
    .dev_name = "zero_file",
    .dev_read = zero_read,
    .dev_write = zero_write,
};
