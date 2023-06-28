#ifndef _PIPE_H
#define _PIPE_H
#include <types.h>

#define PIPE_BUF_SIZE 256
struct thread;
struct Pipe {
	u32 count;
	u64 pipeReadPos;	   // read position
	u64 pipeWritePos;	   // write position
	u8 pipeBuf[PIPE_BUF_SIZE]; // data buffer
	struct thread *waitProc;
};

int pipe(int fd[2]);
#endif
