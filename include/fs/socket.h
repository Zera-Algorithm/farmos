#ifndef _SOCKET_H
#define _SOCKET_H
#include <fs/fd.h>
#include <lock/mutex.h>
#include <types.h>
#include <lib/queue.h>
#include <lock/mutex.h>

#define SOCKET_COUNT 128
#define PENDING_COUNT 128
#define MESSAGE_COUNT 512


#define AF_UNIX 1  /* Unix domain sockets 		*/
#define AF_LOCAL 1 /* POSIX name for AF_UNIX	*/
#define AF_INET 2
#define AF_INET6	10

#define SOCK_STREAM 1
#define SOCK_DGRAM 2

#define SOL_SOCKET 1
#define SO_RCVBUF 8
#define SO_SNDBUF 7

typedef struct SocketAddr {
	u16 family;
	u16 port;
	u32 addr;
	char zero[8];
} SocketAddr;

typedef struct SocketState {
	mutex_t state_lock;
	bool is_close;
} SocketState;


typedef	struct Message {
	TAILQ_ENTRY(Message) message_link;
	u16 family;   // 发送方的family
	u16 port;	// 发送方的port
	u32 addr;	// 发送方的addr
	void * bufferAddr;
	u64 length;
} Message;

typedef TAILQ_HEAD(Message_list, Message) Message_list;


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
	void *bufferAddr;
	SocketState state;
	// bool is_close; 由首先关闭连接的socket来写另一socket的is_close属性
	u64 tid; // 归属的tid

	Message_list messages;
} Socket;

struct Fd;

void socket_init();
int socket(int domain, int type, int protocol);
int bind(int sockfd, const SocketAddr *sockectaddr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int connect(int sockfd, const SocketAddr *addr, socklen_t addrlen);
int accept(int sockfd, SocketAddr *p_addr, socklen_t * addrlen);
void socketFree(int socketNum);

int setsockopt(int sockfd, int lever, int optname, const void * optval, socklen_t optlen);
int getsockopt(int sockfd, int lever, int optname, void * optval, socklen_t * optlen);
int getsocketname(int sockfd, SocketAddr * addr, socklen_t *addrlen);
int sendto(int sockfd, const void * buffer, size_t len, int flags, const SocketAddr * dst_addr, socklen_t *addrlen, int user);
int recvfrom(int sockfd, void *buffer, size_t len, int flgas, SocketAddr * src_addr, socklen_t *addrlen, int user);
int socket_read_check(struct Fd *fd);
int socket_write_check(struct Fd* fd);
int getpeername(int sockfd, SocketAddr * addr, socklen_t* addrlen);

#define SOCKET_TYPE_MASK 0xf
#define SOCK_IS_UDP(type) (((type) & SOCKET_TYPE_MASK) == SOCK_DGRAM)

#endif
