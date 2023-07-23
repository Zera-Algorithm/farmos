#ifndef _SOCKET_H
#define _SOCKET_H
#include <fs/fd.h>
#include <types.h>

#define SOCKET_COUNT 128
#define PENDING_COUNT 128

typedef unsigned int socklen_t;

typedef struct {
	u16 family;
	u16 port;
	u32 addr;
	char zero[8];
} SocketAddr;

typedef struct Socket {
	bool used;
	mutex_t lock;
	u32 type;
	// Process *process;
	SocketAddr addr;	/* local addr */
	SocketAddr target_addr; /* remote addr */
	u64 socketReadPos;	// read position
	u64 socketWritePos;
	SocketAddr waiting_queue[PENDING_COUNT];
	int waiting_h;
	int waiting_t;
	// struct Spinlock lock;
	int listening;
} Socket;

#endif