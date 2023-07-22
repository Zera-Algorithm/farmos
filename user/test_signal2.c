#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <usignal.h>

// 访问错误地址（空指针）的测试

int a[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
int *test = NULL;
void sgv_handler(int num) {
	printf("Segment fault appear!\n");
	test = &a[0];
	printf("test = %d.\n", *test);
	exit(0);
}

int main(int argc, char **argv) {
	sigset_t set;
	sigset_init(&set);
	struct sigaction sig;
	sig.sa_handler = sgv_handler;
	sig.sa_mask = set;
	panic_on(sigaction(11, &sig, NULL));
	printf("I will perform null pointer access.\n");
	*test = 10;
	printf("test = %d.\n", *test);
	return 0;
}
