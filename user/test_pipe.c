#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

static int fd[2];

void test_pipe_zrp(void) {
	TEST_START(__func__);
	int cpid;
	char buf[128] = {0};
	int ret = pipe(fd);
	if (ret == -1) {
		printf("error!");
		exit(0);
	}
	const char *data = "  Write to pipe successfully.\n";
	cpid = fork();
	printf("cpid: %d\n", cpid);
	if (cpid > 0) {
		close(fd[1]);
		while (read(fd[0], buf, 1) > 0)
			write(STDOUT, buf, 1);
		write(STDOUT, "\n", 1);
		close(fd[0]);
		wait(NULL);
	} else {
		close(fd[0]);
		sleep(5);
		write(fd[1], data, strlen(data));
		close(fd[1]);
		exit(0);
	}
	TEST_END(__func__);
}

int main(void) {
	test_pipe_zrp();
	return 0;
}
