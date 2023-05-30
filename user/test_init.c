#include <mm/memlayout.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <trap/syscallDataStruct.h>
#include <unistd.h>

int main() {
	int wstatus = 0;
	while (1) {
		wait(&wstatus);
		sched_yield();
	}
	return 0;
}
