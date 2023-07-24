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

	// busybox的测试点
	char *const *argvs[] = {
	    (char *const[]){"/busybox_unstripped", "find", "-name", "busybox_cmd.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "rm", "busybox_cmd.bak", NULL},
	    (char *const[]){"/busybox_unstripped", "cp", "busybox_cmd.txt", "busybox_cmd.bak",
			    NULL},
	    (char *const[]){"/busybox_unstripped", "grep", "hello", "busybox_cmd.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "rmdir", "test", NULL},
	    (char *const[]){"/busybox_unstripped", "mv", "test_dir", "test", NULL},
	    (char *const[]){"/busybox_unstripped", "mkdir", "test_dir", NULL},
	    (char *const[]){"/busybox_unstripped", "rm", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "more", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "[", "-f", "test.txt", "]", NULL},
	    (char *const[]){"/busybox_unstripped", "wc", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "strings", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "stat", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "sort", "test.txt", "|", "./busybox", "uniq",
			    NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "bbbbbbb", ">>", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "1111111", ">>", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "2222222", ">>", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "aaaaaaa", ">>", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "bbbbbbb", ">>", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "ccccccc", ">>", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "md5sum", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "hexdump", "-C", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "tail", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "head", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "od", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "cut", "-c", "3", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "cat", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "hello world", ">", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "touch", "test.txt", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "#### file opration test", NULL},
	    (char *const[]){"/busybox_unstripped", "sleep", "1", NULL},
	    (char *const[]){"/busybox_unstripped", "ls", NULL},
	    (char *const[]){"/busybox_unstripped", "kill", "10", NULL},
	    (char *const[]){"/busybox_unstripped", "hwclock", NULL},
	    (char *const[]){"/busybox_unstripped", "free", NULL},
	    (char *const[]){"/busybox_unstripped", "pwd", NULL},
	    (char *const[]){"/busybox_unstripped", "ps", NULL},
	    (char *const[]){"/busybox_unstripped", "printf", "abc\n", NULL},
	    (char *const[]){"/busybox_unstripped", "uptime", NULL},
	    (char *const[]){"/busybox_unstripped", "uname", NULL},
	    (char *const[]){"/busybox_unstripped", "which", "ls", NULL},
	    (char *const[]){"/busybox_unstripped", "true", NULL},
	    (char *const[]){"/busybox_unstripped", "false", NULL},
	    (char *const[]){"/busybox_unstripped", "expr", "1", "+", "1", NULL},
	    (char *const[]){"/busybox_unstripped", "du", NULL},
	    (char *const[]){"/busybox_unstripped", "dmesg", NULL},
	    (char *const[]){"/busybox_unstripped", "dirname", "/aaa/bbb", NULL},
	    (char *const[]){"/busybox_unstripped", "df", NULL},
	    (char *const[]){"/busybox_unstripped", "date", NULL},
	    // (char *const[]) {"/busybox_unstripped", "clear", NULL},
	    (char *const[]){"/busybox_unstripped", "cal", NULL},
	    (char *const[]){"/busybox_unstripped", "basename", "/aaa/bbb", NULL},
	    (char *const[]){"/busybox_unstripped", "sh", "-c", "exit", NULL},
	    (char *const[]){"/busybox_unstripped", "ash", "-c", "exit", NULL},
	    (char *const[]){"/busybox_unstripped", "echo", "#### independent command test", NULL},
	    NULL,
	};

	// char *const *argvs[] = {
	// 	(char *const[]) {"/busybox_unstripped", "sh", "busybox_testcode.sh", NULL},
	// 	NULL
	// };

	// // libc-test的测试点
	// char *const *argvs[] = {
	// 	(char *const[]){"/runtest.exe", "-w", "entry-static.exe", "qsort", NULL},
	// 	(char *const[]){"/runtest.exe", "-w", "entry-static.exe", "argv", NULL},
	// 	(char *const[]){"/runtest.exe", "-w", "entry-static.exe", "basename", NULL},
	// 	NULL,
	// }
	char *const envp[] = {"env1=1", "env2=2", NULL};

	int child = fork();
	if (child) {
		wait(&wstatus);
		syscall_shutdown();
	} else {
		// child
		printf("[test_init]: before execve! I'm %x\n", getpid());

		for (int i = 0; argvs[i] != NULL; i++) {
			int pid = fork();
			if (pid == 0) { // child
				continue;
			} else {
				wait(&wstatus);

				// 打印argvs[i]指向的字符串数组
				printf("\n$ ");
				for (int j = 0; argvs[i][j] != NULL; j++) {
					printf("%s ", argvs[i][j]);
				}
				printf("\n");

				execve(argvs[i][0], argvs[i], envp);
			}
		}
	}

	// char *const argv[] = {"/runtest.exe", "-w", "entry-static.exe", "qsort", NULL};
	// char *const argv[] = {"/libc-bench", NULL};
	return 0;
}
