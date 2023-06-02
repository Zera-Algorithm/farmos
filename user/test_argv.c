#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <trap/syscallDataStruct.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	for (int i = 0; i < argc; i++) {
		printf("%s\n", argv[i]);
	}
	printf("test_echo completed! argc = %d\n", argc);
	return 0;
}
