#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <trap/syscallDataStruct.h>
#include <unistd.h>

int main() {
	printf("test_init started!\n");

	int wstatus = 0;
	// char *test_files[] = {
	// 	"/sleep",
	// 	"/times",
	// 	// "/test_echo",
	// 	// "/brk",
	// 	// "/clone",
	// 	// "/execve",
	// 	NULL
	// };

	char *test_files[] = {"/brk", "/chdir",
			      // "/clone",
			      "/close", "/dup", "/dup2", "/execve",
			      // "/exit",
			      // "/fork",
			      "/fstat", "/getcwd", "/getdents", "/getpid", "/getppid",
			      "/gettimeofday",
			      // "/mkdir_",
			      "/mmap",
			      // "/mnt",
			      "/mount", "/munmap", "/open", "/openat",
			      // "/pipe",
			      "/read", "/sleep", "/test_echo",
			      // "/text.txt",
			      "/times", "/umount", "/uname",
			      // "/UNLINK",
			      // "/wait",
			      // "/waitpid",
			      "/write",
			      // "/yield",
			      NULL};

	int child = fork();
	if (child) {
		wait(&wstatus);
		syscall_shutdown();
	} else {
		// child
		char *const argv[] = {"Hello!", "test_echo", NULL};
		char *const envp[] = {NULL};
		// printf("[test_init]: before execve! I'm %x\n", getpid());

		for (int i = 0; test_files[i] != NULL; i++) {
			int pid = fork();
			if (pid == 0) { // child
				continue;
			} else {
				wait(&wstatus);
				execve(test_files[i], argv, envp);
			}
		}

		// // 一个父亲连续fork多个儿子，多个儿子并行执行
		// int p1 = fork();
		// if (p1 != 0) {
		// 	int p2 = fork();
		// 	if (p2 != 0) {
		// 		wait(&wstatus);
		// 		printf("father is OK1!\n");
		// 		wait(&wstatus);
		// 		printf("father is OK2!\n");
		// 	} else {
		// 		execve("/sleep", argv, envp);
		// 	}
		// } else {
		// 	execve("/times", argv, envp);
		// }

		// 一个父亲连续fork多个儿子，等待上一个儿子执行完再fork下一个
		// int p1 = fork();
		// if (p1 != 0) {
		// 	wait(&wstatus);
		// 	printf("father is OK1!\n");
		// 	int p2 = fork();
		// 	if (p2 != 0) {
		// 		wait(&wstatus);
		// 		printf("father is OK2!\n");
		// 	} else {
		// 		execve("/times", argv, envp);
		// 	}
		// } else {
		// 	execve("/sleep", argv, envp);
		// }
	}
	return 0;
}
