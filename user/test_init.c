#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

int main() {
	printf("test_init started!\n");
	int wstatus = 0;

	char *test_files[] = {
	    // 以下测试均能通过
	    "/pipe",	 "/fstat",	  "/mmap",    "/munmap", "/mount",  "/unlink", "/umount",
	    "/mkdir_",	 "/close",	  "/dup",     "/dup2",	 "/open",   "/chdir",  "/openat",
	    "/getdents", "/fork",	  "/write",   "/read",	 "/getcwd", "/brk",    "/getpid",
	    "/getppid",	 "/gettimeofday", "/exit",    "/times",	 "/uname",  "/execve", "/yield",
	    "/wait",	 "/clone",	  "/waitpid", "/sleep",	 NULL};

	int child = fork();
	if (child) {
		wait(&wstatus);
	} else {
		// child
		char *const argv[] = {NULL};
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
