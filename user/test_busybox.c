#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

int main() {
	// 执行 make sdrun 来测试busybox
	printf("test_busybox started!\n");
	int wstatus = 0;

	char *const *argvs[] = {
		(char *const[]) {"/busybox", "ash", "iperf_testcode.sh", NULL},
		/*
		(char *const[]) {"/busybox", "ash", "netperf_testcode.sh", NULL},

		// time-test
	    (char *const[]){"/time-test", NULL},
	    (char *const[]) {"/busybox", "ash", "lmbench_testcode_part.sh", NULL},
		// (char *const[]) {"./lmbench_all", "lat_ctx", "-P", "1", "-s", "32", "2", "4", "8", "16", "24", "32", "64", "96", NULL},
		// libc-bench测试
		(char *const[]){"/libc-bench", NULL},
		(char *const[]) {"/busybox", "ash", "cyclictest_testcode.sh", NULL},

		// busybox测试
	    (char *const[]) {"/busybox", "ash", "busybox_testcode.sh", NULL},

	    // libc-test的static测试点和dynamic测试点
	    (char *const[]) {"/busybox", "ash", "run-static.sh", NULL},
	    (char *const[]) {"/busybox", "ash", "run-dynamic.sh", NULL},

		// iozone

		// lua测试：pass
	    (char *const[]){"/busybox", "ash", "lua_testcode.sh", NULL},
		// lmbench_all lat_ctx -P 1 -s 32 2 4 8 16 24 32 64 96

	    (char *const[]) {"/busybox", "ash", "iozone_testcode.sh", NULL},

		// unixbench测试
		(char *const[]) {"/busybox", "ash", "unixbench_testcode_part.sh", NULL},

	    // (char *const[]) {"/busybox", "ash", "lmbench_testcode.sh", NULL},
		// (char *const[]) {"/busybox", "ash", "netperf_testcode.sh", NULL},

	    // cyclictest测试

	    // lmbench测试
	    (char *const[]) {"/busybox", "ash", "cyclictest_testcode.sh", NULL},
		*/
	    NULL};

	char *const envp[] = {"LD_LIBRARY_PATH=/", "UB_BINDIR=./", NULL};

	int child = fork();
	if (child) {
		wait(&wstatus);
	} else {
		// child
		printf("[test_init]: before execve! I'm %x\n", getpid());

		for (int i = 0; argvs[i] != NULL; i++) {
			int pid = fork();
			if (pid == 0) { // child
				// 打印argvs[i]指向的字符串数组
				printf("\n$ ");
				for (int j = 0; argvs[i][j] != NULL; j++) {
					printf("%s ", argvs[i][j]);
				}
				printf("\n");

				execve(argvs[i][0], argvs[i], envp);
			} else {
				wait(&wstatus);
			}
		}
	}
	printf("!TEST FINISH!\n");
	reboot();
	return 0;
}
