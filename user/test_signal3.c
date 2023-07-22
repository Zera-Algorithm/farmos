#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <usignal.h>

// 内核对用户内存空间的写时复制测试
sigset_t set2;
sigset_t set;

int main(int argc, char **argv) {
	sigset_init(&set);
	sigset_set(&set, 1);
	sigset_set(&set, 2);
	panic_on(sigprocmask(0, &set, NULL));
	sigset_clear(&set, 2);
	int ret = fork();
	if (ret != 0) {
		panic_on(sigprocmask(0, &set2, &set));
		printf("Father: %d.\n", sigset_isset(&set, 2));
	} else {
		printf("Child: %d.\n", sigset_isset(&set, 2));
	}
	return 0;
}
