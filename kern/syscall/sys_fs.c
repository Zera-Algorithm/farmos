#include <fs/fd.h>
#include <fs/file.h>

int sys_write(int fd, u64 buf, size_t count) {
	// todo
	return write(fd, buf, count);
}

int sys_read(int fd, u64 buf, size_t count) {
	return read(fd, buf, count);
}

int sys_openat(int fd, u64 filename, int flags, mode_t mode) {
	return openat(fd, filename, flags, mode);
}
