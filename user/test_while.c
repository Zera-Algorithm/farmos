#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

int main() {
	int cycles = 200;
	while (cycles--) {
		for (int i = 1; i <= 10000000; i++) {
		}
		// printf("cycles = %d", ++cycles);
	}
	return 0;
}
