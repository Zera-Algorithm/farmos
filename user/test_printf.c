#include <stdio.h>
#include <syscall.h>
#include <trap/syscallDataStruct.h>
#include <unistd.h>

int main() {
	struct utsname uts;
	struct tms tms;
	int ret = uname(&uts);

	printf("Hello World!\n");
	printf("ret = %d\n", ret);
	printf("sysname: %s\n", uts.sysname);

	while (1) {
		long time = times(&tms);
		printf("time = %d\n", time);
		for (int i = 1; i <= 100000000; i++)
			;
	}
	return 0;
}
