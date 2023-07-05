#include <fs/fd.h>

int sys_write(int fd, u64 buf, size_t count) {
	// todo
	return write(fd, buf, count);
}
