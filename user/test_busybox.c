#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

int main() {
	// 加载fs.img为sdcard.img来测试busybox
	// char *const argv[] = {"/test_argv.b", NULL};
	// execve(argv[0], argv, envp);

	char *const envp[] = {"env1=1", "env2=2", NULL};
	execve("/busybox_unstripped",
	       (char *const[]){"/busybox_unstripped", "echo", "\"12234234####555###\"", NULL},
	       envp);
	return 0;
}
