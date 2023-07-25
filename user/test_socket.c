#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <syscallDataStruct.h>
#include <unistd.h>

#define AF_INET		2
#define PORT 23
#define BACKLOG 5 // 最大监听数

int main() {
	int iSocketFd = 0;
	int iRecvLen = 0;
	int new_fd = 0;
	char buf[4096] = {0};

	
}
