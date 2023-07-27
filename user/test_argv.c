#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

// Note: 暂时不能使用，因为取消了用户程序加载参数的能力
int main(int argc, char *argv[], char *envp[]) {
	for (int i = 0; i < argc; i++) {
		printf("arg[%d]: %s\n", i, argv[i]);
	}

	// 打印环境变量
	for (int i = 0; envp[i] != NULL; i++) {
		printf("envp[%d]: %s\n", i, envp[i]);
	}

	printf("test_echo completed! argc = %d\n", argc);
	return 0;
}
