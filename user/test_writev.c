#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

int main() {
	// 测试writev
	struct iovec iov[5] = {
	    {.iov_base = "Hello ", .iov_len = 6}, {.iov_base = "world", .iov_len = 5},
	    {.iov_base = "!\n", .iov_len = 2},	  {.iov_base = "I'm ", .iov_len = 4},
	    {.iov_base = "zrp!\n", .iov_len = 5},
	};
	writev(1, iov, 5); // 1 is stdout

	// 测试read
	char str[64];
	int n = read(0, str, 10);
	printf("(read) n = %d\n", n);
	str[n] = '\0';
	printf("(read) content: %s\n", str);

	// 测试readv
	struct iovec iov2[3] = {
	    {.iov_base = str, .iov_len = 10},
	    {.iov_base = str + 10, .iov_len = 6},
	    {.iov_base = str + 16, .iov_len = 6},
	};
	str[22] = '\0';
	n = readv(0, iov2, 3);
	printf("(readv) content: %s\n", str);
	return 0;
}
