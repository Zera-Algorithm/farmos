#include <stddef.h>
#include <stdio.h>
#include <syscall.h>
#include <trap/syscallDataStruct.h>
#include <unistd.h>

int main() {
	int cnt = 0;
	while (1) {
		sleep(1);
		++cnt;
		printf("time: %ds\n", cnt);
		if (cnt == 3)
			break;
	}
	return 0;
}
