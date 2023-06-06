#ifndef _PIPE_H
#define _PIPE_H
#include <types.h>

#define PIPE_BUF_SIZE 256

struct Proc;
struct Pipe {
	u32 count;
	u64 pipeReadPos;	   // read position
	u64 pipeWritePos;	   // write position
	u8 pipeBuf[PIPE_BUF_SIZE]; // data buffer
	struct Proc *waitProc;
};

int pipe(int fd[2]);
#endif
