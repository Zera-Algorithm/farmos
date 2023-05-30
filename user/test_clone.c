#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <trap/syscallDataStruct.h>
#include <unistd.h>

__attribute__((aligned(4096))) char stack[4096];

int child_func() {
	printf("I am a child! pid = %x\n", getpid());
	printf("sleep end\n");
	return 0;
}

int main() {
	int childPid = clone(child_func, NULL, stack, 4096, SIGCHLD);
	int ret, wstatus;
	if (childPid < 0) {
		printf("error %d\n", childPid);
		return 0;
	} else {
		sleep(2);
		printf("father get child id = 0x%x\n", childPid);
		// ret = waitpid(childPid, &wstatus, 1); // WNOHANG
		ret = waitpid(childPid, &wstatus, 0);
		printf("wait end. ret = 0x%x\n", ret);
	}
	return 0;
}
