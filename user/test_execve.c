#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <trap/syscallDataStruct.h>
#include <unistd.h>

int main() {
	char path[] = "test_echo";
	char *const argv[] = {"Hello!", "test_echo", NULL};
	char *const envp[] = {NULL};
	printf("before execve! %d\n", argv[0]);
	execve(path, argv, envp);
	return 0;
}
