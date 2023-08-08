#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
	int fd;
	int dirfd;
	int ret;

	printf("file test\n");

	// 1. 文件读、写、删除测试
	fd = openat(AT_FDCWD, "sample.txt", O_RDWR | O_CREATE);
	assert(fd > 0);
	for (int i = 0; i < 512; i++) {
		char buf[5];
		buf[0] = i / 100 + '0';
		buf[1] = i / 10 % 10 + '0';
		buf[2] = i % 10 + '0';
		buf[3] = '\0';
		write(fd, buf, 3);
		write(fd, ": Hello, world!\n", 16);

		if (i % 16 == 0) {
			printf("write round %d\n", i);
		}
	}
	close(fd);

	fd = openat(AT_FDCWD, "sample.txt", O_RDONLY);
	assert(fd > 0);
	char buf[101];
	int n, sum = 0;
	while ((n = read(fd, buf, 100)) > 0) {
		buf[n] = 0;
		printf("%s", buf);
		sum += n;
	}
	if (sum != 512 * 19) {
		printf("read error: sum = %d\n", sum);
		panic("");
	}
	close(fd);

	ret = unlink("sample.txt");
	assert(ret == 0);

	fd = openat(AT_FDCWD, "sample.txt", O_RDONLY);
	assert(fd < 0);

	// 2. 目录创建测试
	ret = mkdir("mytest_dir", 0);
	assert(ret == 0);
	dirfd = openat(AT_FDCWD, "mytest_dir", O_RDONLY);
	assert(dirfd > 0);
	fd = openat(dirfd, "sample.txt", O_RDWR | O_CREATE);	// 在目录中创建文件
	assert(fd > 0);
	close(fd);

	printf("file test end!\n");
	return 0;
}
