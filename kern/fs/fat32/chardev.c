/**
 * 字符设备
 */

#include <fs/chardev.h>
#include <fs/dirent.h>
#include <fs/file_device.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <mm/kmalloc.h>

extern mutex_t mtx_file;

// reference: file_read
static int chardev_read(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	mtx_lock_sleep(&mtx_file);

	chardev_data_t *pdata = file->dev->data;
	// 预读数据
	if (pdata->read) {
		pdata->read(pdata);
	}

	file->file_size = pdata->size; // 将文件大小写回到宿主文件上
	if (off >= pdata->size) {
		mtx_unlock_sleep(&mtx_file);
		return -E_EXCEED_FILE;
	} else if (off + n > pdata->size) {
		warn("read too much. shorten read length from %d to %d!\n", n, pdata->size - off);
		n = pdata->size - off;
	}
	assert(n != 0);

	if (user) {
		copyOut(dst, (void *)(pdata->str + off), n);
	} else {
		memcpy((void *)dst, (void *)(pdata->str + off), n);
	}

	mtx_unlock_sleep(&mtx_file);
	return n;
}

static int chardev_write(struct Dirent *file, int user, u64 src, uint off, uint n) {
	mtx_lock_sleep(&mtx_file);

	chardev_data_t *pdata = file->dev->data;
	assert(n != 0);
	if (off + n > MAX_CHARDEV_STR_LEN) {
		warn("exceed chardev's max size %d!\n", MAX_CHARDEV_STR_LEN);
		mtx_unlock_sleep(&mtx_file);
		return -1;
	} else if (off + n > pdata->size) {
		pdata->size = off + n;	   // 扩充文件大小
		file->file_size = off + n; // 将文件大小写回到宿主文件上
	}

	if (user) {
		copyIn(src, (void *)(pdata->str + off), n);
	} else {
		memcpy((void *)(pdata->str + off), (void *)src, n);
	}

	// 写入数据之后会调用的函数，用于同步因为数据更改而产生的变更（比如执行一些内核动作）
	if (pdata->write) {
		pdata->write(pdata);
	}

	mtx_unlock_sleep(&mtx_file);
	return n;
}

/**
 * @brief 以字符串创建字符设备
 * @param str 初始字符串，可以为空
 * @param read 读数据之前会调用的函数，用于获取数据，可以为空
 * @param write
 * 写入数据之后会调用的函数，用于同步因为数据更改而产生的变更（比如执行一些内核动作），可以为空
 */
static struct FileDev *create_chardev(char *str, chardev_read_fn_t read, chardev_write_fn_t write) {
	struct FileDev *dev = (struct FileDev *)kmalloc(sizeof(struct FileDev));
	if (dev == NULL) {
		return NULL;
	}
	dev->dev_id = 'c';
	dev->dev_name = "char_dev";
	dev->dev_read = chardev_read;
	dev->dev_write = chardev_write;

	chardev_data_t *p_data = (void *)kvmAlloc();
	p_data->read = read;
	p_data->write = write;

	if (str == NULL) {
		str = "";
	}
	p_data->size = MIN(strlen(str), MAX_CHARDEV_STR_LEN);
	strncpy(p_data->str, str, MAX_CHARDEV_STR_LEN);
	dev->data = p_data;
	return dev;
}

void create_chardev_file(char *path, char *str, chardev_read_fn_t read, chardev_write_fn_t write) {
	extern FileSystem *fatFs;

	Dirent *file;
	panic_on(createFile(fatFs->root, path, &file));
	if (file == NULL) {
		return;
	}
	file->dev = create_chardev(str, read, write);
	if (file->dev == NULL) {
		return;
	}
	file->type = DIRENT_DEV;
}
