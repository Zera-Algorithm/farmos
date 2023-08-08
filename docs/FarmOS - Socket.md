# Socket

## 概述

​	FarmOs实现socket、bind、listen、connect、write、read、sendto、recvfrom、getsocketname、getpeername等socket相关系统调用，支持流Socket、数据报Socket两种socket类型。FarmOS socket 目前仅支持与localhost之间进行本地通信。

## 数据结构

### Socket结构体

```c
typedef struct Socket {
	bool used;
	mutex_t lock;
	u32 type;
	SocketAddr addr;	
	SocketAddr target_addr; 
	u64 socketReadPos;	
	u64 socketWritePos;
	SocketAddr waiting_queue[PENDING_COUNT];
	int waiting_h;
	int waiting_t;
	int listening;
	void *bufferAddr;
	SocketState state;
	u64 tid; 
	Message_list messages;
	int udp_is_connect;
	int opposite;
} Socket;

```

​	Socket结构体包含socket类型 `type`、自身绑定的通信地址 `addr`、连接的对等socket地址 `target_addr`, 自旋锁 `lock`、是否处于监听 `listen` 等属性。流 socket（`type` 类型为 `SOCK_STREAM`）提供了一个可靠的双向的字节流通信信道，我们为每个流socket申请了一页内核缓冲区内存，地址记录在`bufferAddr`，本socket读缓冲区，读的位置记录在 `socketReadPos` ，对等socket写缓冲区，写的位置记录在 `socketWritePos`； 流 socket 的正常工作需要一对相互连接的 socket，因此流 socket 通常被称为面向连接的，由于服务端同时要处理多个客户端的连接请求，我们设置 `waiting_queue`用于记录服务端的未决连接队列(由待连接的客户端地址结构体组成)。数据报 socket（`type` 类型为 `SOCK_DGRAM`）允许数据以被称为数据报的消息的形式进行交换，我们用`Mesage`结构体作为数据报的发送形式，存储数据报的内容，`messages`记录socket收到的数据报。

### SocketAddr结构体

```c
typedef struct SocketAddr {
	u16 family;
	u16 port;
	u32 addr;
	char zero[8];
} SocketAddr;
```

​	socket API 定义了一个大小为16字节的通用地址结构 struct sockaddr，由于FarmOs 的socket只用于实现localhost本地回环，我们只需要记录协议族 `family`、端口号 `port`、地址 `addr`。

### Message 结构体

```c
typedef	struct Message {
	TAILQ_ENTRY(Message) message_link;
	u16 family;   
	u16 port;	
	u32 addr;
	void * bufferAddr;
	u64 length;
} Message;
```

​	`Message` 结构体由发送socket的协议族、端口、地址等，socket在发送数据报时，会申请Meesage结构体，并为报文申请足够的内核存储空间，将起始地址 `bufferAddr` 和报文长度 `length`记录在 `Message`中。

## 系统调用接口概述

### 流socket

​	socket通信的通信双方可分为主动socket(客户端)， 被动socket(服务端)。客户端和服务端进行通信前均需要通过`socket()`系统调用创建一个socket。两个流socket间通讯需要建立连接：服务端通过`bind()`系统将本socket绑在一个地址上，然后通过调用`listen()`通知内核已进入监听状态，准备接入客户端的连接请求。服务端通过调用 `connect()`指定需连接的 socket 的地址建立与服务器的连接。此时，已调用`listen()`的服务端调用`accept()`系统调用进行接受连接。如果在客户端`connet()`前，服务端已经调用`accept()`,此时服务端将进入阻塞。

​	在连接建立完成后，服务端和客户端之间即可进行调用write()、read()进行双向数据传输，直到其中一个使用 close()关闭连接为止。

#### int socket(int domain, int type, int protocol)

​	我们通过位图管理`socket`结构体的分配，当用户调用`socket()`系统调用时，内核会从自动分配一个空闲的`socket`结构体，并申请一个新的内核文件描述符`Fd`。 在FarmOs中， 一个`socket`结构体将会绑定一个`Fd`结构体。分配成功后，返回用户文件描述符。

#### int bind(int sockfd, const SocketAddr *sockectaddr, socklen_t addrlen)

​	服务端通过该系统调用将一个地址绑定在本socket上。在FarmOs中，我们将`socketaddr`用户地址指向的地址结构体中的数据存入`socket`结构体 `addr`属性。

#### int listen(int sockfd, int backlog)

​	服务端通过该系统调用通知内核已进入监听状态，可以监听客户端请求。在FarmOs中，处于监听状态的socket ，其`listen`属性为1。

#### int connect(int sockfd, const SocketAddr *addr, socklen_t addrlen)

​	客户端可以通过`connect()` 系统调用连接服务端。在FarmOs中，客户端发起连接请求时，我们将`addr`用户地址指向的地址结构体中的数据存入本`socket`结构体 `target_addr`属性，并创建一个新的`Socketaddr`结构体，赋值为自身的地址，同时将这个新的结构体插入服务端的`waiting_queue`队列，随后客户端进入睡眠状态，等待服务端连接并唤醒自身。

#### int accept(int sockfd, SocketAddr *p_addr, socklen_t * addrlen)

​	服务端通过`accept()`相应客户端的连接请求。在FarmOs中，服务端socket将遍历自身`waiting_queue`队列，如果队列中没有新的连接地址请求，服务端进程将陷入睡眠，等待发起连接端唤醒；否则，找到最早发送发送请求的客户端地址，建立通信连接。但此时，监听的socket并不直接进行通信，而是新建立一个与监听socket类型、地址等完全相同的传输socket，将其存入新socket 中的 `target_addr`属性。此后，唤醒对端的客户socket，由客户端和新建立的传输socket完成通信过程。在函数返回时，需要将此次连接的对端地址写入 `p_addr`所指向的用户地址。

#### int fd_socket_read(struct Fd *fd, u64 buf, u64 n, u64 offset)

​	相互连接的流socket之间可直接通过read、write进行数据传输。在FarmOs中，与pipe类似，已在 文件系统中的Fd层抽象了socket设备，当与 socket 绑定的Fd 调用 `read` 、 `write`函数时，会自动跳转到 `fd_socket_read`、`fd_socket_write`。

​		在FarmOs中，在socket创建时，会为每个socket分配一页大小的内核缓冲区，`fd_socket_read`将读出本socket的内核缓冲区中的数据，写入用户空间，参考pipe的设计，当缓冲区没有写入并且对等socket没有关闭时，本socket将陷入睡眠状态，直至对等socket写缓冲区后将本进程唤醒。

####  int fd_socket_write(struct Fd *fd, u64 buf, u64 n, u64 offset) 

​	在FarmOs中，`fd_socket_write` 需要将用户地址`buf`指向的内容，写入 `target_addr`对应的对等socket的内核缓冲区。 同理，参考pipe的设计，当没有写完`n`长度或者缓冲区已满，无法再写入，本socket将陷入睡眠状态，直至对等socket读缓冲区后将本进程唤醒。

### 数据报socket

​	数据报socket之间的通信是无连接的，服务端只需要通过`bind()`将自身的socket绑定到一个地址，服务端和客户端之间即可通过`sendto()`、`recvfrom()`进行通信。

#### int sendto(int sockfd, const void * buffer, size_t len, int flags, const SocketAddr *dst_addr, socklen_t *addrlen, int user)

​	当进程调用`sendto()`时，内核将自动申请一个 message 结构体，将数据报内容和发送socket的地址记录在message结构体中。根据传入参数`dst_addr`找到待接收的socket，将message结构体插入接受socket的 `messages` 队列。

#### int recvfrom(int sockfd, void *buffer, size_t len, int flags, SocketAddr *src_addr, socklen_t *addrlen, int user)

​	当进程调用`recvfrom()`时，内核将从本socket的 `messages` 队列选取一个最早发送的信息，将其内容和数据报发送地址返回给用户程序。

### 其余socket系统调用

​	除上述用于socket间通信系统调用外，FarmOs还实现了：

	 - `int socket_read_check(struct Fd *fd)` 检查socket是否可读
	 - `int socket_write_check(struct Fd *fd)`检查socket是否可写
	 - `int getsocketname(int sockfd, SocketAddr * addr, socklen_t *addrlen)`获得 socket地址
	 - `int getpeername(int sockfd, SocketAddr * addr, socklen_t *addrlen)`获得对等连接socket的地址
	 - `int getsockopt(int sockfd, int lever, int optname, void * optval, socklen_t * optlen)`获得socket option属性值

