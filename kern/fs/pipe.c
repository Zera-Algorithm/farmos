#include <fs/fd.h>
#include <fs/pipe.h>
#include <lib/log.h>
#include <lib/string.h>
#include <mm/vmm.h>
#include <proc/thread.h>

#include <proc/cpu.h>
#define myProc() (cpu_this()->cpu_running)

int pipe(int fd[2]) {
	int fd1 = -1, fd2 = -1;
	int kernfd1 = -1, kernfd2 = -1;
	int i;
	u64 pipeAlloc;

	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->fdList[i] == -1) {
			fd1 = i;
			break;
		}
	}
	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (myProc()->fdList[i] == -1 && i != fd1) {
			fd2 = i;
			break;
		}
	}
	if (fd1 < 0 || fd2 < 0) {
		warn("no free fd in proc fdList\n");
		return -1;
	} else {
		kernfd1 = fdAlloc();
		if (kernfd1 < 0) {
			warn("no free fd in os\n");
			return 1;
		}
		kernfd2 = fdAlloc();
		if (kernfd2 < 0) {
			warn("no free fd in os\n");
			freeFd(kernfd1);
			return 1;
		}

		pipeAlloc = kvmAlloc();
		struct Pipe *p = (struct Pipe *)pipeAlloc;
		p->count = 2;
		p->pipeReadPos = 0;
		p->pipeWritePos = 0;
		memset(p->pipeBuf, 0, PIPE_BUF_SIZE);

		fds[kernfd1].dirent = NULL;
		fds[kernfd1].pipe = (struct Pipe *)pipeAlloc;
		fds[kernfd1].type = dev_pipe;
		fds[kernfd1].flags = O_RDONLY;
		fds[kernfd1].offset = 0;
		myProc()->fdList[fd1] = kernfd1;

		fds[kernfd2].dirent = NULL;
		fds[kernfd2].pipe = (struct Pipe *)pipeAlloc;
		fds[kernfd2].type = dev_pipe;
		fds[kernfd2].flags = O_WRONLY;
		fds[kernfd2].offset = 0;
		myProc()->fdList[fd2] = kernfd2;

		fd[0] = fd1;
		fd[1] = fd2;
		return 0;
	}
}
