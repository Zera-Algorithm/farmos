#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

void test_read() {
	int fd = open("./text.txt", 0);
	char buf[256];
	int size = read(fd, buf, 256);
	if (size < 0) {
		printf("error_read!\n");
		exit(0);
	}

	write(STDOUT, buf, size);
	close(fd);
}

int main() {
	struct utsname uts;
	struct tms tms;
	int ret = uname(&uts);

	printf("Hello World!\n");
	printf("ret = %d\n", ret);
	printf("sysname: %s\n", uts.sysname);

	for (int i = 1; i <= 100000000; i++) {
		for (int j = 1; j <= 20; j++) {
			;
		}
	}
	printf("waiting fat32 end.\n");

	test_read();

	while (1) {
		long time = times(&tms);
		printf("time = %d\n", time);
		for (int i = 1; i <= 100000000; i++)
			;
	}
	return 0;
}
