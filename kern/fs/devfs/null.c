/**
 * /dev/null 是一个虚拟设备，它总会将写入的数据丢弃。当需要删除一个 shell 命令的标
 * 准输出和错误时可以将它们重定向到这个文件。从这个设备中读取数据总是会返回文件结
 * 束的错误。
 */

#include <fs/dirent.h>
#include <fs/file_device.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/transfer.h>

/**
 * @brief 每次从/dev/null读取设备总是会返回EOF文件结束错误
 * 成功时返回读取到的字节数(为零表示读到文件描述符),
 此返回值受文件剩余字节数限制.当返回值小于指定的字节数时 并不意味着错误;这可
       能是因为当前可读取的字节数小于指定的 字节数(比如已经接近文件结尾,或者正在从管道或者终端读取数
 据,或者 read()被信号中断).   发 生错误时返回-1,并置 errno
 为相应值.在这种情况下无法得知文件偏移位置是否有变化.
 */
static int null_read(struct Dirent *file, int user, u64 dst, uint off, uint n) {
	// TODO: 在errorno里报告发生了读取到文件尾错误
	return -1;
}

/**
 * @brief null可以无限写入，但会把数据丢弃
 */
static int null_write(struct Dirent *file, int user, u64 src, uint off, uint n) {
	return n;
}

/**
 * null文件设备
 */

struct FileDev file_dev_null = {
    .dev_id = 'f',
    .dev_name = "null_file",
    .dev_read = null_read,
    .dev_write = null_write,
};
