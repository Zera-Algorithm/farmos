#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>
#include <usignal.h>
#define TEST_NUM 2

// 自己给自己发送信号的测试

int global = 0;
void handler(int num) {
	printf("Reach handler, now the signum is %d, depth = %d!\n", num, global);
	global += 1;

	// 最多嵌套8层
	if (global < 8) {
		tkill(0, TEST_NUM);
	}

	global -= 1;
	printf("depth = %d ended!\n", global);
}

int main() {
	sigset_t set;
	sigset_init(&set);
	struct sigaction sig;

	sig.sa_handler = handler;
	sig.sa_sigaction = NULL;
	sig.sa_mask = set;

	panic_on(sigaction(TEST_NUM, &sig, NULL));
	sigset_set(&set, TEST_NUM);

	printf("sigprocmask begins.\n");
	panic_on(sigprocmask(0, &set, NULL));
	tkill(0, TEST_NUM);
	printf("finish send tkill.\n");

	int ans = 0;
	for (int i = 0; i < 10000000; i++) {
		ans += i;
	}
	panic_on(sigprocmask(1, &set, NULL));
	printf("sigprocmask ends.\n");

	printf("global = %d.\n", global);
	return 0;
}
