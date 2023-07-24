#ifndef _SOCKET_H
#define _SOCKET_H
#include <fs/fd.h>
#include <types.h>

#define SOCKET_COUNT 128
#define PENDING_COUNT 128

#define AF_UNIX 1  /* Unix domain sockets 		*/
#define AF_LOCAL 1 /* POSIX name for AF_UNIX	*/

typedef unsigned int socklen_t;

typedef struct SocketAddr{
	u16 family;
	u16 port;
	u32 addr;
	char zero[8];
} SocketAddr;

typedef struct SocketState{
	mutex_t state_lock;
	bool is_close;
} SocketState;

typedef struct Socket {
	bool used;
	mutex_t lock;
	u32 type;
	SocketAddr addr;	/* local addr */
	SocketAddr target_addr; /* remote addr */
	u64 socketReadPos;	// read position
	u64 socketWritePos;
	SocketAddr waiting_queue[PENDING_COUNT];
	int waiting_h;
	int waiting_t;
	int listening;
	void * bufferAddr;
	SocketState state;
	// bool is_close; 由首先关闭连接的socket来写另一socket的is_close属性
} Socket;

void socket_init();
int socket(int domain, int type, int protocol);
int bind(int sockfd, const SocketAddr *sockectaddr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int connect(int sockfd, const SocketAddr *addr, socklen_t addrlen);
int accept(int sockfd, SocketAddr *addr);
void socketFree(int socketNum);

#endif