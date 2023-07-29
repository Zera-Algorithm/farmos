#include <fs/fd.h>
#include <fs/fd_device.h>
#include <fs/socket.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <proc/interface.h>
#include <proc/sleep.h>
#include <proc/sched.h>
#include <lib/queue.h>
#include <proc/tsleep.h>
#include <sys/errno.h>
#include <mm/kmalloc.h>

static uint socket_bitmap[SOCKET_COUNT / 32] = {0};
Socket sockets[SOCKET_COUNT];
struct mutex mtx_socketmap;

Message messages[MESSAGE_COUNT];
Message_list message_free_list;
struct mutex mtx_messages;

static int fd_socket_read(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_socket_write(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_socket_close(struct Fd *fd);
static int fd_socket_stat(struct Fd *fd, u64 pkStat);

static void gen_local_socket_addr();
static Socket *remote_find_peer_socket(const Socket *local_socket);
static Socket *find_listening_socket(const SocketAddr *addr, int type);
static Message * message_alloc();
static Socket * find_remote_socket(SocketAddr * addr, int type, int socket_index);
static void message_free(Message * message);

struct FdDev fd_dev_socket = {
    .dev_id = 's',
    .dev_name = "socket",
    .dev_read = fd_socket_read,
    .dev_write = fd_socket_write,
    .dev_close = fd_socket_close,
    .dev_stat = fd_socket_stat,
};

void socket_init() {
	mtx_init(&mtx_socketmap, "sys_socketable", 1, MTX_SPIN);
	mtx_init(&mtx_messages, "message_alloc", 1, MTX_SPIN);

	for (int i = 0; i < SOCKET_COUNT; i++) {
		mtx_init(&sockets[i].lock, "socket_lock", 1, MTX_SPIN);
		mtx_init(&sockets[i].state.state_lock, "socket_state_lock", 1, MTX_SPIN);
		TAILQ_INIT(&sockets[i].messages);
	}

	// 初始化空闲message队列
	TAILQ_INIT(&message_free_list);
	for (int i = 0; i < MESSAGE_COUNT; i++) {
		TAILQ_INSERT_TAIL(&message_free_list, &messages[i], message_link);
	}
}


int socketAlloc() {
	int i;
	mtx_lock(&mtx_socketmap);
	for (i = 0; i < SOCKET_COUNT; i++) {
		int index = i >> 5;
		int inner = i & 31;
		if ((socket_bitmap[index] & (1 << inner)) == 0) {
			socket_bitmap[index] |= 1 << inner;
			mtx_unlock(&mtx_socketmap);
			return i;
		}
	}
	mtx_unlock(&mtx_socketmap);
	return -1;
}

int socket(int domain, int type, int protocol) {
	int i, usfd = -1;

	for (i = 0; i < MAX_FD_COUNT; i++) {
		if (cur_proc_fs_struct()->fdList[i] == -1) {
			usfd = i;
			break;
		}
	}

	if (usfd < 0) {
		warn("no free fd in proc fdList\n");
		return -1;
	}

	int socketNum = socketAlloc();
	if (socketNum < 0) {
		warn("All socket is used, please check\n");
		return -1;
	}

	//  对socket这个结构体加锁
	Socket *socket = &sockets[socketNum];
	mtx_lock(&socket->lock);
	socket->used = true;
	socket->addr.family = domain;
	socket->type = type;
	memset(&socket->target_addr, 0, sizeof(SocketAddr));
	socket->waiting_h = socket->waiting_t = 0;
	socket->bufferAddr = (void *)kvmAlloc();
	socket->tid = cpu_this()->cpu_running->td_tid;
	TAILQ_INIT(&socket->messages);

	int sfd = fdAlloc();
	if (sfd < 0) {
		warn("All fd in kernel is used, please check\n");
		socketFree(socketNum);
		return -1;
	}

	// fds list加锁
	mtx_lock_sleep(&fds[sfd].lock);
	fds[sfd].dirent = NULL;
	fds[sfd].pipe = NULL;
	fds[sfd].type = dev_socket;
	u64 other_flags = type & ~0xf; // 除了类型之外的其他flags
	fds[sfd].flags = O_RDWR | other_flags;
	fds[sfd].offset = 0;
	fds[sfd].fd_dev = &fd_dev_socket;
	fds[sfd].socket = socket;
	mtx_unlock_sleep(&fds[sfd].lock);


	cur_proc_fs_struct()->fdList[usfd] = sfd;

	mtx_unlock(&socket->lock);
	// 释放socket结构体的锁

	mtx_lock(&socket->state.state_lock);
	socket->state.is_close = false;
	mtx_unlock(&socket->state.state_lock);

	warn("Thread %s: socket create: build socketfd %d, type %d\n", cpu_this()->cpu_running->td_name, usfd, type);
	return usfd;
}

static inline int get_socket_by_fd(int sockfd, Socket **socket) {
	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -EBADF;
	} // 检查Fd类型是否匹配

	*socket = fds[sfd].socket;
	return 0;
}

int bind(int sockfd, const SocketAddr *p_sockectaddr, socklen_t addrlen) {
	SocketAddr socketaddr;
	copyIn((u64)p_sockectaddr, &socketaddr, sizeof(SocketAddr));

	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	} // 检查Fd类型是否匹配

	Socket *socket = fds[sfd].socket;
	warn("Thread %s: bind addr: sockfd = %d, type = %d, family = %d, addr = %d, port = %d\n",
		cpu_this()->cpu_running->td_name, sockfd, socket->type, socketaddr.family, socketaddr.addr, socketaddr.port);

	// if (socket->type == SOCK_DGRAM) {
	// 	printf("bind type = DGRAM\n");
	// }

	mtx_lock(&socket->lock);

	if (socket->addr.family == socketaddr.family) {
		socket->addr.addr = socketaddr.addr;
		socket->addr.port = socketaddr.port;
	}

	mtx_unlock(&socket->lock);

	return 0;
}

int listen(int sockfd, int backlog) {
	// linux listen接口参数有backlog，已完成连接数量与未完成连接数量之和的最大值
	// 但通常为常量128，所以已经在socekt.h中直接给出，可忽略这个参数
	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	} // 检查Fd类型是否匹配

	Socket *socket = fds[sfd].socket;
	warn("Thread %s: listen sockfd = %d, type = %d\n", cpu_this()->cpu_running->td_name, sockfd, socket->type);

	// TODO 对socket进行加锁
	mtx_lock(&socket->lock);
	socket->listening = 1;
	mtx_unlock(&socket->lock);

	// TODO 如何体现无法在一个已连接的 socket（即已经成功执行 connect的 socket
	// 或由accept调用返回的 socket）上执行 listen
	return 0;
}

int connect(int sockfd, const SocketAddr *p_addr, socklen_t addrlen) {
	SocketAddr addr;
	copyIn((u64)p_addr, &addr, sizeof(SocketAddr));

	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	} // 检查Fd类型是否匹配

	Socket *local_socket = fds[sfd].socket;

	warn("Thread %s: connect sockfd = %d, type = %d, family = %d, port = %d, addr = %lx\n",
		cpu_this()->cpu_running->td_name, sockfd, local_socket->type, addr.family, addr.port, addr.addr);

	mtx_lock(&local_socket->lock);
	if (local_socket->addr.port == 0) {
		gen_local_socket_addr(&local_socket->addr);
	}
	local_socket->target_addr = addr;
	mtx_unlock(&local_socket->lock);

	// 如果是UDP，只设置target_addr就可以
	if (SOCK_IS_UDP(local_socket->type)) {
		// printf("connect: type = UDP");
		return 0;
	}

	// TCP的情况：除了要设置对方地址，还需要等待
	Socket *target_socket = find_listening_socket(&addr, local_socket->type & 0xf);
	if (target_socket == NULL) {
		warn("server socket doesn't exists or isn't listening\n");
		return -1;
	}

	// mtx_lock(&target_socket->lock);

	if (target_socket->waiting_t - target_socket->waiting_h == PENDING_COUNT) {
		return -1; // 达到最高限制
	}

	target_socket->waiting_queue[(target_socket->waiting_t++) % PENDING_COUNT] =
	    local_socket->addr;

	int pos = (target_socket->waiting_t - 1 + PENDING_COUNT) % PENDING_COUNT;

	// 尝试唤醒服务端，以等待队列指针作为chan
	wakeup(target_socket->waiting_queue);

	if (local_socket->tid != target_socket->tid) {
		//  释放服务端target_socket的锁，客户端进入睡眠，等待服务端唤醒客户端
		sleep(&target_socket->waiting_queue[pos], &target_socket->lock,
			"connecting... waiting for target socket to accept\n");
	}

	mtx_unlock(&target_socket->lock);

	return 0;
}

int accept(int sockfd, SocketAddr *p_addr, socklen_t * addrlen) {
	SocketAddr addr;

	int newUerFd;
	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	} // 检查Fd类型是否匹配

	Socket *local_socket = fds[sfd].socket;

	mtx_lock(&local_socket->lock);
	// 得到服务端socket的锁

	while (local_socket->waiting_h == local_socket->waiting_t) {
		// 此时代表没有连接请求，释放服务端socket锁，进入睡眠
		// 等到客户端有请求时，唤醒服务端，并且被唤醒后，此时服务端拥有服务端socket的锁
		sleep(local_socket->waiting_queue, &local_socket->lock,
		      "waiting for socket to enter waiting queue...\n");
	}

	// Socket *addr = socket->waiting_queue[(socket->waiting_h++) % PENDING_COUNT];

	newUerFd = socket(local_socket->addr.family, local_socket->type, 0);
	if (newUerFd < 0) {
		return -1;
	}

	Socket *newSocket = fds[cur_proc_fs_struct()->fdList[newUerFd]].socket;

	newSocket->addr = local_socket->addr;
	newSocket->target_addr =
	    local_socket->waiting_queue[(local_socket->waiting_h++) % PENDING_COUNT];
	addr = newSocket->target_addr;

	//  释放服务端socket锁，唤醒newsocket对应的客户端
	int pos = (local_socket->waiting_h - 1 + PENDING_COUNT) % PENDING_COUNT;
	wakeup(&local_socket->waiting_queue[pos]);
	mtx_unlock(&local_socket->lock);

	copyOut((u64)p_addr, &addr, sizeof(SocketAddr));
	return newUerFd;
}

static void gen_local_socket_addr(SocketAddr * socket_addr) {
	static u32 local_addr = (127 << 24) + 1;
	static u32 local_port = 10000;
	socket_addr->addr = local_addr;
	socket_addr->port = local_port++;
}

/**
 * @brief 查找指定地址和类型的监听socket
 */
static Socket *find_listening_socket(const SocketAddr *addr, int type) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		mtx_lock(&sockets[i].lock);
		if (sockets[i].used &&
			// sockets[i].addr.family == addr->family && // family可能未必相同，可以一个是INET一个是INET6
		    // sockets[i].addr.addr == addr->addr &&
			sockets[i].type == type &&
			sockets[i].addr.port == addr->port &&
		    sockets[i].listening) {
			// mtx_unlock(&sockets[i].lock);
			return &sockets[i];
		}
		mtx_unlock(&sockets[i].lock);
	}
	return NULL;
	// TODO 加type参数即可
}

static Socket *remote_find_peer_socket(const Socket *local_socket) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		mtx_lock(&sockets[i].lock);
		if ( sockets[i].used &&
			local_socket - sockets != i && // not self
			// sockets[i].addr.family == local_socket->target_addr.family &&
		    sockets[i].addr.port == local_socket->target_addr.port &&
		    // sockets[i].addr.addr == local_socket->target_addr.addr &&
		    sockets[i].target_addr.port == local_socket->addr.port // &&
		    // sockets[i].target_addr.addr == local_socket->addr.addr
			) {
			// mtx_unlock(&sockets[i].lock);
			return &sockets[i];
		}
		mtx_unlock(&sockets[i].lock);
	}
	return NULL;
	// TODO 加type参数即可
}

static int fd_socket_read(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	warn("Thread %s: socket read, fd = %d, n = %d\n", cpu_this()->cpu_running->td_name, fd - fds, n);
	int i;
	char ch;
	char *readPos;
	Socket *localSocket = fd->socket;

	// UDP：直接从对端地址读取
	if (SOCK_IS_UDP(localSocket->type)) {
		return recvfrom(fd - fds, (void *)buf, n, 0, &localSocket->target_addr, NULL, 0);
	}

	// TCP
	mtx_lock(&localSocket->lock);
	// 读时，对自身socket进行加自旋锁

	while (localSocket->socketReadPos == localSocket->socketWritePos) {
		mtx_lock(&localSocket->state.state_lock);
		if (!localSocket->state.is_close) {
			mtx_unlock(&localSocket->state.state_lock);

			wakeup(&localSocket->socketWritePos);
			mtx_unlock_sleep(&fd->lock);
			sleep(&localSocket->socketReadPos, &localSocket->lock,
			      "wait another socket to write");
			mtx_lock_sleep(&fd->lock);
		} else {
			mtx_unlock(&localSocket->state.state_lock);
			break;
		}
	}

	for (i = 0; i < n; i++) {
		if (localSocket->socketReadPos == localSocket->socketWritePos) {
			break;
		}
		readPos =
		    (char *)(localSocket->bufferAddr + ((localSocket->socketReadPos) % PAGE_SIZE));
		ch = *readPos;
		// TODO：成批拷贝，加快速度。否则会将时间浪费在查页表上
		copyOut((buf + i), &ch, 1);
		localSocket->socketReadPos++;
	}
	fd->offset += i;

	wakeup(&localSocket->socketWritePos);
	mtx_unlock(&localSocket->lock);

	return i;
}

static int fd_socket_write(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	warn("thread %s: socket write, fd = %d, n = %d\n", cpu_this()->cpu_running->td_name, fd - fds, n);
	int i = 0;
	char ch;
	char *writePos;
	Socket *localSocket = fd->socket;

	// UDP
	if (SOCK_IS_UDP(localSocket->type)) {
		return sendto(fd - fds, (void *)buf, n, 0, &localSocket->target_addr, NULL, 0);
	}

	// TCP
	Socket *targetSocket = remote_find_peer_socket(localSocket);
	if (targetSocket == NULL) {
		warn("socket write error: can\'t find target socket.\n");
		// 原因可能是远端已关闭
		return -EPIPE;
	}

	// mtx_lock(&targetSocket->lock);

	while (i < n) {
		mtx_lock(
		    &localSocket->state.state_lock); // 获得自身socket的状态锁，从而来获得targetSocket是否关闭的状态
		if (targetSocket->socketWritePos - targetSocket->socketReadPos == PAGE_SIZE) {
			if (localSocket->state.is_close /* 对面socket进程已结束*/) {
				mtx_unlock(&localSocket->state.state_lock);
				mtx_unlock(&targetSocket->lock);
				warn("socket writer can\'t write more.\n");
				return -EPIPE;
			} else {
				mtx_unlock(&localSocket->state.state_lock);

				wakeup(&targetSocket->socketReadPos);
				mtx_unlock_sleep(&fd->lock);
				sleep(&targetSocket->socketWritePos, &targetSocket->lock,
				      "wait another socket to  read.\n");
				mtx_lock_sleep(&fd->lock);
			}
		} else {
			mtx_unlock(&localSocket->state.state_lock);

			copyIn((buf + i), &ch, 1);
			writePos = (char *)(targetSocket->bufferAddr +
					    ((targetSocket->socketWritePos) % PAGE_SIZE));
			if (writePos == NULL) {
				asm volatile("ebreak");
			}
			*writePos = ch;
			targetSocket->socketWritePos++;
			i++;
		}
	}

	fd->offset += i;
	wakeup(&targetSocket->socketReadPos);
	mtx_unlock(&targetSocket->lock);
	return i;
}

void socketFree(int socketNum) {

	Socket *socket = &sockets[socketNum];

	mtx_lock(&socket->lock);
	socket->used = false;
	socket->socketReadPos = 0;
	socket->socketWritePos = 0;
	socket->listening = 0;
	socket->waiting_h = 0;
	socket->waiting_t = 0;
	socket->addr.family = 0;
	socket->type = 0;

	if (socket->bufferAddr != NULL) {
		kvmFree((u64)socket->bufferAddr);
		socket->bufferAddr = NULL;
	}

	memset(&socket->addr, 0, sizeof(SocketAddr));
	memset(&socket->target_addr, 0, sizeof(SocketAddr));
	memset(socket->waiting_queue, 0, (sizeof(SocketAddr) * PENDING_COUNT));

	Message * message;
	while (!TAILQ_EMPTY(&socket->messages)) {
		message = TAILQ_FIRST(&socket->messages);
		TAILQ_REMOVE(&socket->messages, message, message_link);
		message_free(message);
	}

	mtx_unlock(&socket->lock);

	mtx_lock(&socket->state.state_lock);
	socket->state.is_close = false;
	mtx_unlock(&socket->state.state_lock);

	mtx_lock(&mtx_socketmap);
	//  对socketBitMap加锁，使得只有一个进程能够进入Alloc函数

	assert(socketNum >= 0 && socketNum < SOCKET_COUNT);
	int index = socketNum >> 5;
	int inner = socketNum & 31;
	socket_bitmap[index] &= ~(1 << inner);
	mtx_unlock(&mtx_socketmap);
} // TODO free socket时需要释放messages剩余的message

static int fd_socket_close(struct Fd *fd) {
	warn("thread %s: socket close, fd = %d\n", cpu_this()->cpu_running->td_name, fd - fds);

	Socket *localSocket = fd->socket;
	Socket *targetSocket = remote_find_peer_socket(localSocket);
	if (targetSocket != NULL) {
		mtx_unlock(&targetSocket->lock);
		mtx_lock(&targetSocket->state.state_lock);
		targetSocket->state.is_close = true;
		mtx_unlock(&targetSocket->state.state_lock);
	}

	wakeup(&localSocket->socketWritePos); // TODO 检查此处wake的正确性
	int socketNum = localSocket - sockets;
	socketFree(socketNum);

	return 0;
}

static int fd_socket_stat(struct Fd *fd, u64 pkStat) {
	return 0;
}

// udpdata struct
static Message * message_alloc() {
	Message * message;
	mtx_lock(&mtx_messages);
	if (TAILQ_EMPTY(&message_free_list)) {
		warn("no free message struct\n");
		return NULL;
	}
	message = TAILQ_FIRST(&message_free_list);
	TAILQ_REMOVE(&message_free_list, message, message_link);
	mtx_unlock(&mtx_messages);

	message->message_link.tqe_next= NULL;
	message->message_link.tqe_prev = NULL;
	message->bufferAddr = (void *)kmalloc(4000);
	message->length = 0;

	return message;
}


static Socket * find_remote_socket(SocketAddr * addr, int self_type, int socket_index) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		mtx_lock(&sockets[i].lock);
		if ( sockets[i].used &&
			i != socket_index && // 不能找到自己
			sockets[i].type == self_type &&
			// sockets[i].addr.family == addr->family &&
		    sockets[i].addr.port == addr->port // &&
		    // sockets[i].addr.addr == addr->addr
		) {
			mtx_unlock(&sockets[i].lock);
			return &sockets[i];
		}
		mtx_unlock(&sockets[i].lock);
	}
	return NULL;
} // TODO type判断还有些问题

static void message_free(Message * message) {
	message->message_link.tqe_next = NULL;
	message->message_link.tqe_prev = NULL;
	message->family = 0;
	message->port = 0;
	message->addr = 0;
	message->length = 0;
	kfree(message->bufferAddr);

	mtx_lock(&mtx_messages);
	TAILQ_INSERT_TAIL(&message_free_list, message, message_link);
	mtx_unlock(&mtx_messages);
}

/**
 * @brief 接收用户数据报，将对方地址存入src_addr
 * @param user 当user为0时，表示内核调用。
 * 此时，sockfd为内核fd号，src_addr为内核地址。读取src_addr中的值而不是写入，且仅接受从src_addr发来的报文
 */
int recvfrom(int sockfd, void *buffer, size_t len, int flags, SocketAddr *src_addr, socklen_t *addrlen, int user) {
	// int r;
	int min_size;
	Socket *local_socket;

	if (user) {
		int sfd = cur_proc_fs_struct()->fdList[sockfd];

		if (sfd >= 0 && fds[sfd].type != dev_socket) {
			warn("target fd is not a socket fd, please check\n");
			return -1;
		} // 检查Fd类型是否匹配

		local_socket = fds[sfd].socket;
	} else {
		local_socket = fds[sockfd].socket;
	}
	warn("thread %s: socket recvfrom, fd = %d\n", cpu_this()->cpu_running->td_name, sockfd);

	SocketAddr socketaddr;
	if (!user) socketaddr = *src_addr;

	Message * message = NULL;
	Message * msg;
	mtx_lock(&local_socket->lock);
	while (message == NULL) {
		if (!user) {
			TAILQ_FOREACH (msg, &local_socket->messages, message_link) {
				if (
					// msg->addr == socketaddr.addr &&
					// msg->family == socketaddr.family &&
					msg->port == socketaddr.port) {
						message = msg;
						break;
					}
			}
		} else {
			message = TAILQ_FIRST(&local_socket->messages);
		}

		if (message == NULL) {
			// TODO：wakeup
			sleep(&local_socket->messages, &local_socket->lock,
			      "wait another UDP socket to write");
			// tsleep(&local_socket->messages, &local_socket->lock, "wait another UDP socket to write", 10000);
		} else {
			min_size = MIN(len, message->length);
			copyOut((u64)buffer, message->bufferAddr,  min_size);

			// 向用户态返回对方地址
			socketaddr.family = message->family;
			socketaddr.port = message->port;
			socketaddr.addr = message->addr;
			if (user && src_addr) copyOut((u64)src_addr, &socketaddr, sizeof(SocketAddr));
			break;
		}
	}

	TAILQ_REMOVE(&local_socket->messages, message, message_link);

	mtx_unlock(&local_socket->lock);

	message_free(message);

	return min_size;

}

int sendto(int sockfd, const void * buffer, size_t len, int flags, const SocketAddr *dst_addr, socklen_t *addrlen, int user) {
	Socket *local_socket;
	if (user) {
		int sfd = cur_proc_fs_struct()->fdList[sockfd];

		if (sfd >= 0 && fds[sfd].type != dev_socket) {
			warn("target fd is not a socket fd, please check\n");
			return -EBADF;
		}

		local_socket = fds[sfd].socket;
	} else {
		local_socket = fds[sockfd].socket;
	}

	SocketAddr socketaddr;
	if (user) {
		copyIn((u64)dst_addr, &socketaddr, sizeof(SocketAddr));
	} else {
		socketaddr = *dst_addr;
	}
	warn("sendto addr: %x, port: %d\n", socketaddr.addr, socketaddr.port);

	Socket * target_socket = find_remote_socket(&socketaddr, local_socket->type, local_socket - sockets);

	if (target_socket == NULL) {
		warn("target addr socket doesn't exists\n");
		return MIN(len, 65535); // 发送不了也不报错，因为UDP不校验发送的正确性
	}

	Message * message = message_alloc();
	if (message == NULL) {
		return -1;
	}

	message->family = local_socket->addr.family;
	message->addr = local_socket->addr.addr;
	message->port = local_socket->addr.port;

	int min_len = MIN(len, 65535);
	copyIn((u64)buffer, message->bufferAddr, min_len);
	message->length = min_len;

	mtx_lock(&target_socket->lock);
	TAILQ_INSERT_TAIL(&target_socket->messages, message, message_link);
	wakeup(&target_socket->messages); // 唤醒对端的recvfrom
	mtx_unlock(&target_socket->lock);

	return min_len;
}

int getsocketname(int sockfd, SocketAddr * addr, socklen_t *addrlen) {

    int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	}
	socklen_t len = sizeof(SocketAddr);
	Socket *local_socket = fds[sfd].socket;

    if (addr) copyOut((u64)addr, &local_socket->addr, sizeof(SocketAddr));
	if (addrlen) copyOut((u64)addrlen, &len, sizeof(len));
    return 0;
}

int getpeername(int sockfd, SocketAddr * addr, socklen_t *addrlen) {
	Socket *localSocket;
	unwrap(get_socket_by_fd(sockfd, &localSocket));
	// Socket *targetSocket = remote_find_peer_socket(localSocket);
	// assert(targetSocket != NULL);
	socklen_t len = sizeof(SocketAddr);

	if (addr) copyOut((u64)addr, &localSocket->target_addr, sizeof(SocketAddr));
	if (addrlen) copyOut((u64)addrlen, &len, sizeof(len));
	return 0;
}

int getsockopt(int sockfd, int lever, int optname, void * optval, socklen_t * optlen) {
	int size = 4;
	int val;

	if (lever == SOL_SOCKET) {
		if (optname == SO_RCVBUF) {
			val = 131072;
			copyOut((u64)optval, &val, sizeof(socklen_t));
		} else if (optname == SO_SNDBUF) {
			val = 16384;
			copyOut((u64)optval, &val, sizeof(socklen_t));
		}
		copyOut((u64)optlen, &size, sizeof(socklen_t));
	}
	return 0;
}

int setsockopt(int sockfd, int lever, int optname, const void * optval, socklen_t optlen) {
	return 0;
}

/**
 * @brief 可以读返回1，否则为0
 * 1. 如果是监听的socket(listening)，那么返回当前是否有等待的连接
 * 2. 如果是普通的socket，那么返回当前是否有数据可读，或者说socket是否关闭
 *  2.1 TCP 检查buffer是否为空
 *  2.2 UDP 检查消息队列是否为空
 */
int socket_read_check(struct Fd *fd) {
	Socket *socket = fd->socket;
	int ret;
	mtx_lock(&socket->lock);

	if (socket->listening) {
		ret = (socket->waiting_h != socket->waiting_t);
	} else if ((socket->type & 0xf) == SOCK_STREAM) {
		// TCP
		mtx_lock(&socket->state.state_lock);
		ret = ((socket->socketReadPos != socket->socketWritePos) || (socket->state.is_close));
		mtx_unlock(&socket->state.state_lock);
	} else {
		// UDP
		ret = (!TAILQ_EMPTY(&socket->messages));
	}

	mtx_unlock(&socket->lock);
	return ret;
}


/**
 * @brief 可以写返回1，否则为0
 * 1. 如果是监听的socket，返回0
 * 1. 如果是普通的socket，那么返回当前是否可写入数据，或者说socket是否关闭
 *  2.1 TCP 检查buffer是否为满
 *  2.2 UDP 返回1（UDP始终可以发送）
 */
int socket_write_check(struct Fd* fd) {
	Socket *socket = fd->socket;
	int ret;
	mtx_lock(&socket->lock);

	if (socket->listening) {
		ret = 0;
	} else if ((socket->type & 0xf) == SOCK_STREAM) {
		// TCP
		mtx_lock(&socket->state.state_lock);
		ret = ((socket->socketWritePos - socket->socketReadPos != PAGE_SIZE) || (socket->state.is_close));
		mtx_unlock(&socket->state.state_lock);
	} else {
		// UDP
		ret = 1;
	}

	mtx_unlock(&socket->lock);
	return ret;
}
