#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

static int fd[2];
static char mntpoint[64] = "./mnt";
static char device[64] = "/dev/vda2";
static const char *fs_type = "vfat";

int mount(const char *special, const char *dir, const char *fstype, unsigned long flags,
	  const void *data);

char buf2[512];
void test_pipe_zrp(void) {
	TEST_START(__func__);
	int cpid;
	char buf[128] = {0};
	int ret = pipe(fd);
	struct linux_dirent64 *dirp64 = (struct linux_dirent64 *)buf2;

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

		printf("Mounting dev:%s to %s\n", device, mntpoint);
		ret = mount(device, mntpoint, fs_type, 0, NULL);
		printf("mount return: %d\n", ret);

		int dirFd = open("/mnt", O_RDONLY);
		for (int i = 0; i < 4; i++) {
			int nread = getdents(dirFd, dirp64, 512);
			printf("getdents fd:%d\n", nread);
			printf("getdents success.\n%s\n", dirp64->d_name);
		}
	} else {
		close(fd[0]);
		sleep(1);
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
