#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_REQUEUE 3
#define FUTEX_PRIVATE_FLAG 128

int stack[2][1024];
int f[4];

int main1() {
	printf("test_futex started, I'm %x in main1()\n", gettid());
	// 等 f0 释放
	futex(&f[0], FUTEX_WAIT, 0, 0, 0, 0);
	printf("test_futex started, I'm %x in main1() after get futex 0\n", gettid());
	// 释放 f1
	futex(&f[1], FUTEX_WAKE, 1, 0, 0, 0);
	exit(0);
}

int main2() {
	printf("test_futex started, I'm %x in main2()\n", gettid());
	// 等 f1 释放
	futex(&f[1], FUTEX_WAIT, 1, 0, 0, 0);
	printf("test_futex started, I'm %x in main2() after get futex 1\n", gettid());
	// 释放 f2
	futex(&f[2], FUTEX_WAKE, 1, 0, 0, 0);
	exit(0);
}

int main3() {
	// 测试超时
	struct ts {
		long sec;
		long nsec;
	} ts;
	ts.sec = 1;
	ts.nsec = 0;
	printf("test_futex started, I'm %x in main3()\n", gettid());
	// 等 f0 释放
	futex(&f[0], FUTEX_WAIT, 0, &ts, 0, 0);
	printf("test_futex started, I'm %x in main3() after timeout\n", gettid());
	exit(0);
}

int main4() {
	printf("test_futex started, I'm %x in main4()\n", gettid());
	// 等 f0 释放
	futex(&f[0], FUTEX_WAIT, 0, 0, 0, 0);
	printf("test_futex started, I'm %x in main4() after get futex\n", gettid());
	exit(0);
}

int main5() {
	printf("test_futex started, I'm %x in main5()\n", gettid());
	// 等 f0 释放
	futex(&f[1], FUTEX_WAIT, 0, 0, 0, 0);
	printf("test_futex started, I'm %x in main5() after get futex\n", gettid());
	exit(0);
}

int main0() {
	printf("test_futex started, I'm %x in main0()\n", gettid());

	// 测试1：2个子线程都wait，链式唤醒
	int tid1 = clone(main1, NULL, stack[0], 1024, CLONE_VM);
	int tid2 = clone(main2, NULL, stack[1], 1024, CLONE_VM);

	sleep(2);
	printf("test_futex started, I'm %x in main0() after sleep\n", gettid());
	// 释放 f0
	futex(&f[0], FUTEX_WAKE, 1, 0, 0, 0);
	printf("test_futex started, I'm %x in main0() after release futex 0\n", gettid());
	// 等 f2 释放
	futex(&f[2], FUTEX_WAIT, 1, 0, 0, 0);

	waitpid(tid1, 0, 0);
	waitpid(tid2, 0, 0);
	printf("tid1 = %d, tid2 = %d\n", tid1, tid2);
	printf("test_futex(normal) finished!\n");

	// 测试2：超时自动唤醒
	int tid3 = clone(main3, NULL, stack[0], 1024, CLONE_VM);
	waitpid(tid3, 0, 0);
	printf("tid3 = %d\n", tid3);
	printf("test_futex(timeout) finished!\n");

	sleep(5);

	// 测试3：requeue，t4 等 f[0]，t5 等 f[1]，把 f[0] 换到 f[1]，唤醒 f[1]，再唤醒 f[1]，t4 和
	// t5 都会被唤醒
	int tid4 = clone(main4, NULL, stack[0], 1024, CLONE_VM);
	int tid5 = clone(main5, NULL, stack[1], 1024, CLONE_VM);
	sleep(2);
	// 把 tid4 的 futex 从 f[0] 换到 f[1]
	futex(&f[0], FUTEX_REQUEUE, 0, 0, &f[1], 0);
	sleep(2);
	// 唤醒 f[1];
	futex(&f[1], FUTEX_WAKE, 1, 0, 0, 0);
	sleep(2);
	// 唤醒 f[1];
	futex(&f[1], FUTEX_WAKE, 1, 0, 0, 0);
	waitpid(tid4, 0, 0);
	waitpid(tid5, 0, 0);
	printf("tid4 = %d, tid5 = %d\n", tid4, tid5);
	printf("test_futex(requeue) finished!\n");

	return 0;
}

int main() {
	printf("test_futex started!\n");
	exit(main0());
}
