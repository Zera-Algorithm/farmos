#include <dev/sbi.h>
#include <fs/fd.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <proc/proc.h>

static uint fdBitMap[FDNUM / 32] = {0};
struct Fd fds[FDNUM];
uint citesNum[FDNUM] = {0};

static int readconsole = -1;
static int writeconsole = -1;
static int errorconsole = -1;

void freeFd(uint i);

int readConsoleAlloc() {
	if (readconsole == -1) {
		readconsole = fdAlloc();
		fds[readconsole].type = dev_console;
		fds[readconsole].flags = O_RDONLY;
	} else {
		citesNum[readconsole] += 1;
	}
	return readconsole;
}

int writeConsoleAlloc() {
	if (writeconsole == -1) {
		writeconsole = fdAlloc();
		fds[writeconsole].type = dev_console;
		fds[writeconsole].flags = O_WRONLY;
	} else {
		citesNum[writeconsole] += 1;
	}
	return writeconsole;
}

int errorConsoleAlloc() {
	if (errorconsole == -1) {
		errorconsole = fdAlloc();
		fds[errorconsole].type = dev_console;
		fds[errorconsole].flags = O_RDWR;
	} else {
		citesNum[errorconsole] += 1;
	}
	return errorconsole;
}

int fdAlloc() {
	uint i;
	for (i = 0; i < FDNUM; i++) {
		int index = i >> 5;
		int inner = i & 31;
		if ((fdBitMap[index] & (1 << inner)) == 0) {
			fdBitMap[index] |= 1 << inner;
			citesNum[i] = 1;
			;
			return i;
		}
	}
	return -1;
}

/**
 * @param i 内核fd编号
 */
void cloneAddCite(uint i) {
	assert(i >= 0 && i < FDNUM);
	citesNum[i] += 1; // 0 <= i < 1024
}

int closeFd(int fd) {
	int kernFd;
	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("close param fd is wrong, please check\n");
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd is wrong, please check\n");
			return -1;
		} else {
			kernFd = myProc()->fdList[fd];
			freeFd(kernFd);
			myProc()->fdList[fd] = -1;
			return 0;
		}
	}
}

/**
 * @brief 将内核fd引用计数减一
 */
void freeFd(uint i) {
	assert(i >= 0 && i < FDNUM);
	citesNum[i] -= 1;
	if (citesNum[i] == 0) {
		int index = i >> 5;
		int inner = i & 31;
		fdBitMap[index] &= ~(1 << inner);
		fds[i].dirent = NULL;
		fds[i].type = 0;
		fds[i].offset = 0;
		fds[i].flags = 0;
		memset(&fds[i].stat, 0, sizeof(struct kstat));
	}
}

int read(int fd, u64 buf, size_t count) {
	int kernFd;
	Dirent *dirent;
	int n, i;
	char ch;
	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("read param fd is wrong, please check\n");
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd is wrong, please check\n");
			return -1;
		} else {
			kernFd = myProc()->fdList[fd];
			if ((fds[kernFd].flags & O_ACCMODE) == O_WRONLY) {
				warn("fd can not be read\n");
				return -1;
			}
			if (fds[kernFd].type == dev_file) {
				dirent = fds[kernFd].dirent;
				n = fileRead(dirent, 1, buf, fds[kernFd].offset, count);
				if (n < 0) {
					warn("file read num is below zero\n");
					return -1;
				}
				fds[kernFd].offset += n;
				return n;
			} else if (fds[kernFd].type == dev_console) {
				for (i = 0; i < count; i++) {
					if ((ch = SBI_GETCHAR()) < 0) {
						return -1;
					}
					copyOut((buf + i), &ch, 1);
				}
				fds[kernFd].offset += count;
				return count;
			} else {
				return -1;
			}
		}
	}
}

int write(int fd, u64 buf, size_t count) {
	int kernFd;
	Dirent *dirent;
	int n, i;
	char ch;
	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("write param fd is wrong, please check\n");
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd is wrong, please check\n");
			return -1;
		} else {
			kernFd = myProc()->fdList[fd];
			if ((fds[kernFd].flags & O_ACCMODE) == O_RDONLY) {
				warn("fd can not be write\n");
				return -1;
			}
			if (fds[kernFd].type == dev_file) {
				dirent = fds[kernFd].dirent;
				n = fileWrite(dirent, 1, buf, fds[kernFd].offset, count);
				if (n < 0) {
					warn("file read num is below zero\n");
					return -1;
				}
				fds[kernFd].offset += n;
				return n;
			} else if (fds[kernFd].type == dev_console) {
				for (i = 0; i < count; i++) {
					copyIn((buf + i), &ch, 1);
					SBI_PUTCHAR(ch);
				}
				fds[kernFd].offset += count;
				return count;
			} else {
				// pipe
				return -1;
			}
		}
	}
}

int dup(int fd) {
	int newFd = -1;
	int kernFd;
	int i;

	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("dup param fd is wrong, please check\n");
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd is wrong, please check\n");
			return -1;
		}
	}
	kernFd = myProc()->fdList[fd];
	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->fdList[i] == -1) {
			newFd = i;
			break;
		}
	}
	if (newFd < 0) {
		warn("no free fd in proc fdList\n");
		return -1;
	}

	myProc()->fdList[newFd] = kernFd;
	citesNum[kernFd] += 1;
	return newFd;
}

int dup3(int old, int new) {
	int copied;

	if (old < 0 || old >= MAX_FD_COUNT) {
		warn("dup param old is wrong, please check\n");
		return -1;
	}
	if (new < 0 || new >= MAX_FD_COUNT) {
		warn("dup param new[] is wrong, please check\n");
		return -1;
	}
	if (myProc()->fdList[new] >= 0 && myProc()->fdList[new] < FDNUM) {
		freeFd(myProc()->fdList[new]);
	} else if (myProc()->fdList[new] >= FDNUM) {
		warn("kern fd is wrong, please check\n");
		return -1;
	}
	if (myProc()->fdList[old] < 0 || myProc()->fdList[old] >= FDNUM) {
		warn("kern fd is wrong, please check\n");
		return -1;
	}
	copied = myProc()->fdList[old];
	myProc()->fdList[new] = copied;
	citesNum[copied] += 1;

	return new;
}

static int getDirentByFd(int fd, Dirent **dirent, int *kernFd) {
	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("write param fd is wrong, please check\n");
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd is wrong, please check\n");
			return -1;
		} else {
			int kFd = myProc()->fdList[fd];
			if (kernFd)
				*kernFd = kFd;
			if (dirent)
				*dirent = fds[kFd].dirent;
			return 0;
		}
	}
}

int getdents64(int fd, u64 buf, int len) {
	Dirent *dir, *file;
	int kernFd, ret, offset;
	unwrap(getDirentByFd(fd, &dir, &kernFd));

	DirentUser direntUser;
	direntUser.d_ino = 0;
	direntUser.d_reclen = DIRENT_USER_SIZE;
	direntUser.d_type = dev_file;
	ret = dirGetDentFrom(dir, fds[kernFd].offset, &file, &offset, NULL);
	direntUser.d_off = offset;
	fds[kernFd].offset = offset;

	strncpy(direntUser.d_name, file->name, DIRENT_NAME_LENGTH);

	if (ret == 0) {
		warn("read dirents to the end! dir: %s\n", dir->name);
	} else {
		ret = DIRENT_USER_SIZE;
	}
	copyOut(buf, &direntUser, DIRENT_USER_SIZE);

	return ret;
}
