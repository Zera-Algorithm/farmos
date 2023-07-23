#include <dev/sbi.h>
#include <fs/fd.h>
#include <fs/fd_device.h>
#include <lib/error.h>
#include <lib/printf.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <proc/sched.h>

static int fd_console_read(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_console_write(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_console_close(struct Fd *fd);
static int fd_console_stat(struct Fd *fd, u64 pkStat);

// 定义console设备访问函数
// 内部的函数均应该是可重入函数，即没有static变量，不依赖全局变量（锁除外）
struct FdDev fd_dev_console = {
    .dev_id = 'c',
    .dev_name = "console",
    .dev_read = fd_console_read,
    .dev_write = fd_console_write,
    .dev_close = fd_console_close,
    .dev_stat = fd_console_stat,
};

// 以下代码用于分配并初始化进程的0,1,2号文件描述符
static int readconsole = -1;
static int writeconsole = -1;
static int errorconsole = -1;

int readConsoleAlloc() {
	if (readconsole == -1) {
		readconsole = fdAlloc();
		fds[readconsole].type = dev_console;
		fds[readconsole].flags = O_RDONLY;
		fds[readconsole].fd_dev = &fd_dev_console;
	} else {
		cloneAddCite(readconsole);
	}
	return readconsole;
}

int writeConsoleAlloc() {
	if (writeconsole == -1) {
		writeconsole = fdAlloc();
		fds[writeconsole].type = dev_console;
		fds[writeconsole].flags = O_WRONLY;
		fds[writeconsole].fd_dev = &fd_dev_console;
	} else {
		cloneAddCite(writeconsole);
	}
	return writeconsole;
}

int errorConsoleAlloc() {
	if (errorconsole == -1) {
		errorconsole = fdAlloc();
		fds[errorconsole].type = dev_console;
		fds[errorconsole].flags = O_RDWR;
		fds[errorconsole].fd_dev = &fd_dev_console;
	} else {
		cloneAddCite(errorconsole);
	}
	return errorconsole;
}

static int fd_console_read(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	char ch;
	for (int i = 0; i < n; i++) {
		// 如果没读到字符，sbi_getchar会返回255
		while ((ch = SBI_GETCHAR()) == 255) {
			yield();
		}
		// printf("sbi_getchar: %c\n", ch);
		copyOut((buf + i), &ch, 1);
	}
	fd->offset += n;
	return n;
}

static int fd_console_write(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	if (n >= 1024) {
		panic("console write too much: n = %ld", n);
	}

	char s[1024];
	copyIn(buf, s, n);
	s[n] = 0;

	// 需要在copyIn之后再printf输出，防止先获取pr_lock造成死锁
	printf("%s", s);

	fd->offset += n;
	return n;
}

/**
 * @brief 从设备侧关闭console
 * @note 并不清理Fd结构体
 */
static int fd_console_close(struct Fd *fd) {
	return 0;
}

// TODO
static int fd_console_stat(struct Fd *fd, u64 pkStat) {
	return 0;
}
