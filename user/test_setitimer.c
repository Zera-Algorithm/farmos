#include <stdio.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>
#include <usignal.h>

void itimer_handler(int num) {
	printf("Reach handler, now the signum is %d!\n", num);
}

int main() {
	sigset_t set;
	sigset_init(&set);
	struct sigaction sig;

	sig.sa_handler = itimer_handler;
	sig.sa_mask = set;

	sigaction(SIGALRM, &sig, NULL);
	struct itimerval it;

	// 1s后开始，每1s触发一次
	it.it_value.tv_sec = 10;
	it.it_value.tv_usec = 0;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	setitimer(0, &it, NULL);
	while (1);
	return 0;
}
