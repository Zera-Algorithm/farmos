#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

int main() {
	printf("test_init started!\n");
	int wstatus = 0;

	char *test_files[] = {"/chdir", "/fstat", "/mkdir_", "/mmap", "/mount", "/munmap", "/pipe",
			      "/test_echo", "/umount", "/unlink",

			      // 以下测试均能通过
			      "/brk", "/close", "/dup", "/dup2", "/exit", "/open", "/openat",
			      "/getcwd", "/getdents", "/getpid", "/getppid", "/gettimeofday",
			      "/write", "/read", "/times", "/uname", "/execve", "/yield", "/wait",
			      "/clone", "/fork", "/waitpid", "/sleep", NULL};

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
		// for (int i = 0; test_files[i] != NULL; i++) {
		// 	printf("start fork!\n\n");
		// 	int pid = fork();
		// 	if (pid == 0) { // child
		// 		printf("I'm son, start test exec!\n\n");
		// 		execve(test_files[i], argv, envp);
		// 		continue;
		// 	} else { // father
		// 		printf("son %d started, wait!\n\n", pid);
		// 		wait(&wstatus);
		// 		printf("son %d exited, run next test!\n\n", pid);
		// 	}
		// }
	}
	return 0;
}
