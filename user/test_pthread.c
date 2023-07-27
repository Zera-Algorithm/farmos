#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

#define THREAD_FLAGS                                                                                                   \
	(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |             \
	 CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID | CLONE_DETACHED)

__attribute__((aligned(4096))) char stack1[4096];
__attribute__((aligned(4096))) char stack2[4096];

int global = 0;

int child_func1() {
	for (int i = 1; i <= 1000000; i++) {
		for (int j = 1; j <= 20; j++) {
			;
		}
	}
	global += 1;
	printf("I am a child1! pid = %x, global = %d\n", getpid(), global);
	printf("sleep end\n");
	return 0;
}

int child_func2() {
	for (int i = 1; i <= 1000000; i++) {
		for (int j = 1; j <= 30; j++) {
			;
		}
	}
	global += 1;
	printf("I am a child2! pid = %x, global = %d\n", getpid(), global);
	printf("sleep end\n");
	return 0;
}

int main() {
	int childPid;
	childPid = clone(child_func1, NULL, stack1, 4096, THREAD_FLAGS);
	childPid = clone(child_func2, NULL, stack2, 4096, THREAD_FLAGS);
	if (childPid < 0) {
		printf("error %d\n", childPid);
		return 0;
	} else {
		sleep(2);
		printf("father get child id = 0x%x, global = %d\n", childPid, global);
	}
	return 0;
}
