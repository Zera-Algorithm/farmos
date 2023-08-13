#include <fs/dirent.h>
#include <fs/file_device.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/transfer.h>
#include <fs/console.h>

static int tty_read(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	return console_read(dst, n);
}

/**
 * @brief tty可以无限写入
 */
static int tty_write(struct Dirent *file, int user, u64 src, uint off, uint n) {
	return console_write(src, n);
}

/**
 * tty文件设备
 */

struct FileDev file_dev_tty = {
    .dev_id = 'f',
    .dev_name = "tty_file",
    .dev_read = tty_read,
    .dev_write = tty_write,
};
