#include <dev/sbi.h>
#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/pipe.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <proc/proc.h>
#include <proc/sleep.h>

static uint fdBitMap[FDNUM / 32] = {0};
struct Fd fds[FDNUM];
uint citesNum[FDNUM] = {0};

static int readconsole = -1;
static int writeconsole = -1;
static int errorconsole = -1;

void freeFd(uint i);
int pipeIsClose(int fd);

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
	struct Pipe *p;
	assert(i >= 0 && i < FDNUM);
	citesNum[i] -= 1;
	if (citesNum[i] == 0) {
		// TODO 这里后续要继续特殊判，是file对应的fd关闭还是pipe对应的fd关闭
		// TODO 如果是file,是否要继续考虑怎么回收Dirent
		// TODO 如果是pipe对应的fd关闭，则需要回收struct pipe对应的内存
		int index = i >> 5;
		int inner = i & 31;
		fdBitMap[index] &= ~(1 << inner);
		if (fds[i].type == dev_pipe) {
			p = fds[i].pipe;
			p->count -= 1;
			if (p && p->count == 0) {
				kvmFree((u64)fds[i].pipe); //释放pipe结构体所在的物理内存
			}
		}
		fds[i].dirent = NULL;
		fds[i].pipe = NULL;
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
	struct Pipe *p;

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
				// fd 的类型是 pipe
				p = fds[kernFd].pipe;
				for (i = 0; i < count; i++) {
					while (p->pipeReadPos == p->pipeWritePos) {
						if (i > 0 || pipeIsClose(fd) == 1) {
							// TODO
							// 返回之前判断是否写端正在阻塞，是，就唤醒
							fds[kernFd].offset += i;
							return i;
						} else {
							// TODO
							// 这里意思是读不了，需要阻塞读pipe的进程
							warn("pipe is empty, can\'t be read. "
							     "sleep!\n");

							p->waitProc = myProc();
							myProc()->pipeWait.i = i;
							myProc()->pipeWait.p = p;
							myProc()->pipeWait.kernFd = kernFd;
							myProc()->pipeWait.count = count;
							myProc()->pipeWait.buf = buf;
							myProc()->pipeWait.fd = fd;
							naiveSleep(myProc(), "pipe");
							return -1;
						}
					}
					ch = p->pipeBuf[p->pipeReadPos % PIPE_BUF_SIZE];
					copyOut((buf + i), &ch, 1);
					p->pipeReadPos++;
				}
				// TODO 返回之前判断是否写端正在阻塞，是，就唤醒
				fds[kernFd].offset += count;
				return count;
			}
		}
	}
}

static void __onWakeup(struct Proc *proc) {
	naiveWakeup(proc);

	int i = proc->pipeWait.i;
	struct Pipe *p = proc->pipeWait.p;
	int kernFd = proc->pipeWait.kernFd;
	int count = proc->pipeWait.count;
	u64 buf = proc->pipeWait.buf;
	int fd = proc->pipeWait.fd;

	for (i = 0; i < count; i++) {
		while (p->pipeReadPos == p->pipeWritePos) {
			if (i > 0 || pipeIsClose(fd) == 1) {
				fds[kernFd].offset += i;
				proc->trapframe->a0 = i;
				return;
			} else {
				warn("pipe is broken, can\'t be read. sleep!\n");
				proc->trapframe->a0 = -1;
				return;
			}
		}
		char ch = p->pipeBuf[p->pipeReadPos % PIPE_BUF_SIZE];
		copyOut((buf + i), &ch, 1);
		p->pipeReadPos++;
	}
	fds[kernFd].offset += count;
	proc->trapframe->a0 = count;
	return;
}

int write(int fd, u64 buf, size_t count) {
	int kernFd;
	Dirent *dirent;
	int n, i;
	char ch;
	struct Pipe *p;
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
				p = fds[kernFd].pipe;
				for (i = 0; i < count; i++) {
					while (p->pipeWritePos - p->pipeReadPos == PIPE_BUF_SIZE) {
						if (pipeIsClose(fd) == 1) {
							// TODO
							// wirte返回值需要查看linux手册，确认没写完且读端关闭的返回值
							fds[kernFd].offset += i;
							return i;
						} else {
							// TODO
							// 这里意思是写不了，需要阻塞写pipe的进程
							return -1;
						}
					}
					copyIn((buf + i), &ch, 1);
					p->pipeBuf[p->pipeWritePos % PIPE_BUF_SIZE] = ch;
					p->pipeWritePos++;
				}
				// TODO 判断读端是否阻塞，是，就唤醒读端
				fds[kernFd].offset += count;

				if (p->waitProc) {
					log(LEVEL_GLOBAL, "wakeup a process!\n");
					__onWakeup(p->waitProc);
					p->waitProc = NULL;
				}

				return count;
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

int getDirentByFd(int fd, Dirent **dirent, int *kernFd) {
	if (fd == AT_FDCWD) {
		if (dirent)
			*dirent = myProc()->cwd;
		return 0;
	}

	if (fd < 0 || fd >= MAX_FD_COUNT) {
		warn("write param fd(%d) is wrong, please check\n", fd);
		return -1;
	} else {
		if (myProc()->fdList[fd] < 0 || myProc()->fdList[fd] >= FDNUM) {
			warn("kern fd(%d) is wrong, please check\n", myProc()->fdList[fd]);
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

int makeDirAtFd(int dirFd, u64 path, int mode) {
	Dirent *dir;
	char name[MAX_NAME_LEN];

	unwrap(getDirentByFd(dirFd, &dir, NULL));
	copyInStr(path, name, MAX_NAME_LEN);

	log(LEVEL_GLOBAL, "make dir %s at %s\n", name, dir->name);
	return makeDirAt(dir, name, mode);
}

int linkAtFd(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags) {
	struct Dirent *oldDir, *newDir;
	char oldPath[MAX_NAME_LEN];
	char newPath[MAX_NAME_LEN];
	unwrap(getDirentByFd(oldFd, &oldDir, NULL));
	unwrap(getDirentByFd(newFd, &newDir, NULL));
	copyInStr(pOldPath, oldPath, MAX_NAME_LEN);
	copyInStr(pNewPath, newPath, MAX_NAME_LEN);
	return linkAt(oldDir, oldPath, newDir, newPath);
}

int unLinkAtFd(int dirFd, u64 pPath) {
	struct Dirent *dir;
	char path[MAX_NAME_LEN];
	unwrap(getDirentByFd(dirFd, &dir, NULL));
	copyInStr(pPath, path, MAX_NAME_LEN);
	return unLinkAt(dir, path);
}

int fileStatFd(int fd, u64 pkstat) {
	struct Dirent *file;
	unwrap(getDirentByFd(fd, &file, NULL));
	struct kstat kstat;
	fileStat(file, &kstat);
	copyOut(pkstat, &kstat, sizeof(struct kstat));
	return 0;
}

int pipeIsClose(int fd) {
	// 由调用者保证fd一定有效
	int kernFd = myProc()->fdList[fd];
	struct Pipe *p = fds[kernFd].pipe;
	if (p != NULL) {
		if (p->count <= 1) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}
