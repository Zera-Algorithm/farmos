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
#include <lib/profiling.h>

// 整体TODO：判别是否为非阻塞模式（fd->flags & O_NONBLOCK），
// 如果是，直接返回错误码(-EAGAIN)，不阻塞
// 涉及的syscall：read, write, connect, accept(已实现), recvfrom, sendto

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
static Socket * find_udp_remote_socket(SocketAddr * addr, int type, int socket_index);
static void message_free(Message * message);
static Socket * find_udp_connect_socket(SocketAddr * addr, int self_type, int socket_index);

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
		// TODO: debug socket_lock重入的问题
		mtx_init(&sockets[i].lock, "socket_lock", 1, MTX_SPIN | MTX_RECURSE);
		mtx_init(&sockets[i].state.state_lock, "socket_state_lock", 1, MTX_SPIN | MTX_RECURSE);
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
	socket->type = (type & 0xf);
	memset(&socket->target_addr, 0, sizeof(SocketAddr));
	socket->waiting_h = socket->waiting_t = 0;
	socket->bufferAddr = (void *)kmalloc(SOCKET_BUFFER_SIZE);
	socket->tid = cpu_this()->cpu_running->td_tid;
	socket->udp_is_connect = 0;
	socket->opposite = -1;
	socket->self_read_close = false;
	socket->self_write_close = false;

	TAILQ_INIT(&socket->messages);

	mtx_lock(&socket->state.state_lock);
	socket->state.is_close = false;
	socket->state.opposite_write_close = false;
	mtx_unlock(&socket->state.state_lock);

	int sfd = fdAlloc();
	if (sfd < 0) {
		warn("All fd in kernel is used, please check\n");
		mtx_unlock(&socket->lock);
		socketFree(socketNum);
		return -1;
	}

	mtx_unlock(&socket->lock);
	// 释放socket结构体的锁

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

	warn("Thread %s: socket create: build socketfd %d, type %d\n", cpu_this()->cpu_running->td_name, sfd, type);
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
		cpu_this()->cpu_running->td_name, sfd, socket->type, socketaddr.family, socketaddr.addr, socketaddr.port);

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
	warn("Thread %s: listen sockfd = %d, type = %d\n", cpu_this()->cpu_running->td_name, sfd, socket->type);

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
		return -EBADF;
	} // 检查Fd类型是否匹配

	Socket *local_socket = fds[sfd].socket;

	warn("Thread %s: sockfd = %d  type = %d,  connect to socket: family = %d, port = %d, addr = %lx\n",
		cpu_this()->cpu_running->td_name, sfd, local_socket->type, addr.family, addr.port, addr.addr);

	mtx_lock(&local_socket->lock);
	if (local_socket->addr.port == 0) {
		gen_local_socket_addr(&local_socket->addr);
	}
	local_socket->target_addr = addr;
	mtx_unlock(&local_socket->lock);

	// 1. connected, opposite

	// 如果是UDP，只设置target_addr就可以
	if (SOCK_IS_UDP(local_socket->type)) {
		Socket *target_socket = find_udp_connect_socket(&addr, local_socket->type & 0xf, local_socket - sockets);
		if (target_socket == NULL) {
			// asm volatile("ebreak"); //
			warn("server socket doesn't exists or isn't listening\n");
			// return -1;
			return 0;
		}
		mtx_lock(&local_socket->lock);
		local_socket->udp_is_connect = 1;
		local_socket->opposite = target_socket - sockets;
		mtx_unlock(&local_socket->lock);
		// printf("connect: type = UDP");
		return 0;
	}

	// TCP的情况：除了要设置对方地址，还需要等待
	Socket *target_socket = find_listening_socket(&addr, local_socket->type & 0xf);
	if (target_socket == NULL) {
		// asm volatile("ebreak"); //
		warn("server socket doesn't exists or isn't listening\n");
		return -1;
	}

	// mtx_lock(&target_socket->lock);

	if (target_socket->waiting_t - target_socket->waiting_h == PENDING_COUNT) {
		warn("fd %d: target socket's pending queue is full\n", sockfd);
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

	// 如果是无阻塞模式下的socket，那么直接返回即可
	if ((fds[sfd].flags & O_NONBLOCK) && local_socket->waiting_h == local_socket->waiting_t) {
		mtx_unlock(&local_socket->lock);
		return -EAGAIN;
	}

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
	warn("Thread %s: sockfd = %d, accept socket addr = %x, port = %d\n", cpu_this()->cpu_running->td_name, sfd, addr.addr, addr.port);
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
			(sockets[i].type & 0xf) == type &&
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

// 如果找到了对端socket，就持有锁
static Socket *remote_find_peer_socket(const Socket *local_socket) {
	// TODO：存在并发安全问题
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
	// int i;
	// char ch;
	// char *readPos;
	Socket *localSocket = fd->socket;

	// UDP：直接从对端地址读取
	if (SOCK_IS_UDP(localSocket->type)) {
		return recvfrom(fd - fds, (void *)buf, n, 0, &localSocket->target_addr, NULL, 0);
	}

	// TCP
	mtx_lock(&localSocket->lock);
	// 读时，对自身socket进行加自旋锁

	if (localSocket->self_read_close) {
		mtx_unlock(&localSocket->lock);
		return -EPIPE;
	}
	while (localSocket->socketReadPos == localSocket->socketWritePos) {
		mtx_lock(&localSocket->state.state_lock);
		if (!localSocket->state.is_close && !localSocket->state.opposite_write_close) {
			mtx_unlock(&localSocket->state.state_lock);

			wakeup(&localSocket->socketWritePos);

			PROFILING_START
			// log(999, "[%ld] %s read sleep, wait socket to write\n", time_rtc_us(), cpu_this()->cpu_running->td_name);
			sleep(&localSocket->socketReadPos, &localSocket->lock,
			      "wait another socket to write");
			// log(999, "[%ld] %s read wakeup, wait socket to write\n", time_rtc_us(), cpu_this()->cpu_running->td_name);
			PROFILING_END_WITH_NAME("socket read sleep")

		} else {
			mtx_unlock(&localSocket->state.state_lock);
			warn("target has closed or target writer is closed.");
			break;
		}
	}

	PROFILING_START

	u64 socket_volumn = localSocket->socketWritePos - localSocket->socketReadPos; // 实际容量
	u64 read_volumn = MIN(n, socket_volumn);
	u64 read_dst = localSocket->socketReadPos + read_volumn;

	// 读取数据
	u64 read_begin = localSocket->socketReadPos % SOCKET_BUFFER_SIZE;
	u64 read_end = read_dst % SOCKET_BUFFER_SIZE;

	if (read_volumn != 0) {
		if (read_begin < read_end) {
			copyOut(buf, localSocket->bufferAddr + read_begin, read_volumn);
		} else {
			copyOut(buf, localSocket->bufferAddr + read_begin, SOCKET_BUFFER_SIZE - read_begin);
			copyOut(buf + SOCKET_BUFFER_SIZE - read_begin, localSocket->bufferAddr, read_end);
		}
	}


	localSocket->socketReadPos += read_volumn;
	fd->offset += read_volumn;

	wakeup(&localSocket->socketWritePos);
	mtx_unlock(&localSocket->lock);
	PROFILING_END_WITH_NAME("socket read actual data")

	return read_volumn;
}

static int fd_socket_write(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	u64 begin_time = time_rtc_us();

	warn("thread %s: socket write, fd = %d, n = %d\n", cpu_this()->cpu_running->td_name, fd - fds, n);
	int i = 0;

	Socket *localSocket = fd->socket;

	// UDP
	if (SOCK_IS_UDP(localSocket->type)) {
		return sendto(fd - fds, (void *)buf, n, 0, &localSocket->target_addr, NULL, 0);
	}

	// TCP
	mtx_lock(&localSocket->lock);
	if (localSocket->self_write_close) {
		mtx_unlock(&localSocket->lock);
		return -EPIPE;
	}
	mtx_unlock(&localSocket->lock);

	Socket *targetSocket = remote_find_peer_socket(localSocket);
	if (targetSocket == NULL || targetSocket->self_read_close) {
		warn("socket write error: can\'t find target socket.\n");
		// 原因可能是远端已关闭,或者远端关闭读
		return -EPIPE;
	}

	mtx_lock(&localSocket->state.state_lock); // 获得自身socket的状态锁，从而来获得targetSocket是否关闭的状态

	while (i < n) {
		// mtx_lock(
		//     &localSocket->state.state_lock); // 获得自身socket的状态锁，从而来获得targetSocket是否关闭的状态
		if (localSocket->state.is_close /* 对面socket进程已结束*/) {
			if (i == 0)	{
				warn("socket write error: target socket is closed.\n");
				mtx_unlock(&localSocket->state.state_lock);
				mtx_unlock(&targetSocket->lock);
				return -EPIPE;
			} else {
				warn("socket writer can\'t write more.\n");
				mtx_unlock(&localSocket->state.state_lock);
				break;
			}
		}
		// 对端肯定没有关闭,   但不一定没有关闭读， 此时对面有可能已经关闭了读
		if (targetSocket->self_read_close) {
			warn("socket writer can\'t write more because target is close reader\n");
			mtx_unlock(&localSocket->state.state_lock);
			break;
			// TODO 可以改成写入非0 break 没有写入 return -EPIPE
		} else {
			if (targetSocket->socketWritePos - targetSocket->socketReadPos == SOCKET_BUFFER_SIZE) {
				mtx_unlock(&localSocket->state.state_lock);

				wakeup(&targetSocket->socketReadPos);
				PROFILING_START
				// log(999, "[%ld] %s write sleep, wait socket to read\n", time_rtc_us(), cpu_this()->cpu_running->td_name);
				u64 _start = time_rtc_us();
				sleep(&targetSocket->socketWritePos, &targetSocket->lock,
						"wait another socket to  read.\n");
				// log(999, "[%ld] %s write wakeup, wait socket to read\n", time_rtc_us(), cpu_this()->cpu_running->td_name);
				begin_time += (time_rtc_us() - _start);
				PROFILING_END_WITH_NAME("socket write sleep")

				mtx_lock(&localSocket->state.state_lock);
			} else {
				u64 left_size = SOCKET_BUFFER_SIZE - (targetSocket->socketWritePos - targetSocket->socketReadPos);
				u64 write_length = MIN(left_size, n);
				u64 write_dst = targetSocket->socketWritePos + write_length;

				// 当write_begin < write_end, 写[write_begin, write_end)

				u64 write_begin = targetSocket->socketWritePos % SOCKET_BUFFER_SIZE;
				u64 write_end = write_dst % SOCKET_BUFFER_SIZE;

				if (write_begin < write_end) {
					copyIn(buf + i, targetSocket->bufferAddr + write_begin, write_length);
				} else {
					copyIn(buf + i, targetSocket->bufferAddr + write_begin,  SOCKET_BUFFER_SIZE - write_begin);
					copyIn(buf + i + SOCKET_BUFFER_SIZE - write_begin, targetSocket->bufferAddr,  write_end);
				}
				i += write_length;
				targetSocket->socketWritePos += write_length;
				// mtx_unlock(&localSocket->state.state_lock);
			}
		}
	}

	mtx_unlock(&localSocket->state.state_lock);

	fd->offset += i;
	wakeup(&targetSocket->socketReadPos);
	mtx_unlock(&targetSocket->lock);
	return i;
}

void socketFree(int socketNum) {

	Socket *socket = &sockets[socketNum];

	mtx_lock(&socket->lock);
	if (socket->listening != 0) {
		warn("closing listening socket %d\n", socketNum);
	}
	socket->used = false;
	socket->socketReadPos = 0;
	socket->socketWritePos = 0;
	socket->listening = 0;
	socket->waiting_h = 0;
	socket->waiting_t = 0;
	socket->addr.family = 0;
	socket->type = 0;

	if (socket->bufferAddr != NULL) {
		kfree(socket->bufferAddr);
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

	mtx_lock(&socket->state.state_lock);
	socket->state.is_close = false;
	mtx_unlock(&socket->state.state_lock);

	mtx_unlock(&socket->lock);

	mtx_lock(&mtx_socketmap);
	//  对socketBitMap加锁，使得只有一个进程能够进入Alloc函数

	assert(socketNum >= 0 && socketNum < SOCKET_COUNT);
	int index = socketNum >> 5;
	int inner = socketNum & 31;
	socket_bitmap[index] &= ~(1 << inner);
	mtx_unlock(&mtx_socketmap);
}

static int fd_socket_close(struct Fd *fd) {
	Socket *localSocket = fd->socket;
	warn("thread %s: socket close, fd = %d, listening = %d\n",
		cpu_this()->cpu_running->td_name, fd - fds, localSocket->listening);
	Socket *targetSocket = remote_find_peer_socket(localSocket);
	if (targetSocket != NULL) {
		mtx_lock(&targetSocket->state.state_lock);
		targetSocket->state.is_close = true;
		mtx_unlock(&targetSocket->state.state_lock);
		wakeup(&targetSocket->socketReadPos);
		mtx_unlock(&targetSocket->lock);
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
		mtx_unlock(&mtx_messages);
		return NULL;
	}
	message = TAILQ_FIRST(&message_free_list);
	TAILQ_REMOVE(&message_free_list, message, message_link);
	mtx_unlock(&mtx_messages);

	message->message_link.tqe_next= NULL;
	message->message_link.tqe_prev = NULL;
	message->bufferAddr = (void *)kvmAlloc();
	message->length = 0;

	return message;
}

// designed only by UDP
static Socket * find_udp_connect_socket(SocketAddr * addr, int self_type, int socket_index) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		mtx_lock(&sockets[i].lock);
		if ( sockets[i].used &&
			i != socket_index && // 不能找到自己
			sockets[i].type == self_type &&
			// sockets[i].addr.family == addr->family &&
		    (sockets[i].addr.port == addr->port)// &&
		    // sockets[i].addr.addr == addr->addr
			&& (!sockets[i].udp_is_connect || (sockets[i].udp_is_connect && sockets[i].opposite == socket_index))
		) {
			mtx_unlock(&sockets[i].lock);
			return &sockets[i];
		}
		mtx_unlock(&sockets[i].lock);
	}
	return NULL;
} // TODO type判断还有些问题

// designed only by UDP
static Socket * find_udp_remote_socket(SocketAddr * addr, int self_type, int socket_index) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		mtx_lock(&sockets[i].lock);
		if ( sockets[i].used &&
			i != socket_index && // 不能找到自己
			sockets[i].type == self_type &&
			// sockets[i].addr.family == addr->family &&
		    (sockets[i].addr.port == addr->port &&(!sockets[i].udp_is_connect || (sockets[i].udp_is_connect && sockets[i].opposite == socket_index)))
		    // sockets[i].addr.addr == addr->addr
			&& (!sockets[i].udp_is_connect || (sockets[i].udp_is_connect && sockets[i].opposite == socket_index))
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
	kvmFree((u64)message->bufferAddr);

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

	int ws = 0;
	if (user) {
		int sfd = cur_proc_fs_struct()->fdList[sockfd];

		if (sfd < 0 || (sfd >= 0 && fds[sfd].socket == NULL)) {
			warn("target fd is not a socket fd, please check\n");
			return -1;
		}

		local_socket = fds[sfd].socket;
		if (local_socket->type == 1) {
			return fd_socket_read(&fds[sfd], (u64)buffer,  (u64)len, 0);
		}
		ws = sfd;
	} else {
		local_socket = fds[sockfd].socket;
		ws = sockfd;
	}

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

	warn("thread %s: socket fd = %d recvfrom addr: %x, port %d\n", cpu_this()->cpu_running->td_name, ws, socketaddr.addr, socketaddr.port);
	return min_size;

}

int sendto(int sockfd, const void * buffer, size_t len, int flags, const SocketAddr *dst_addr, socklen_t *addrlen, int user) {
	PROFILING_START
	Socket *local_socket;
	int ws;
	if (user) {
		int sfd = cur_proc_fs_struct()->fdList[sockfd];

		if (sfd < 0) {
			warn("kernFd is below 0!");
			return -EBADF;
		} else if (sfd >= 0 && fds[sfd].type != dev_socket) {
			warn("target fd is not a socket fd, please check\n");
			return -EBADF;
		}

		local_socket = fds[sfd].socket;
		if (local_socket == NULL) {
			asm volatile("nop");
		}
		assert(local_socket != NULL);
		if (local_socket->type == 1) {
			return fd_socket_write(&fds[sfd], (u64)buffer,  (u64)len, 0);
		}
		ws = sfd;
	} else {
		local_socket = fds[sockfd].socket;
		ws = sockfd;
	}

	SocketAddr socketaddr;
	if (user) {
		copyIn((u64)dst_addr, &socketaddr, sizeof(SocketAddr));
	} else {
		socketaddr = *dst_addr;
	}


	Socket * target_socket = find_udp_remote_socket(&socketaddr, local_socket->type, local_socket - sockets);

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

	warn("thread %s: socket fd = %d sendto addr: %x, port %d\n", cpu_this()->cpu_running->td_name, ws, socketaddr.addr, socketaddr.port);

	PROFILING_END
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
		if (socket->self_read_close) {
			ret = 1;
		} else {
			mtx_lock(&socket->state.state_lock);
			ret = ((socket->socketReadPos != socket->socketWritePos) || (socket->state.is_close) || (socket->state.opposite_write_close));
			mtx_unlock(&socket->state.state_lock);
		}
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
 * 2. 如果是普通的socket，那么返回当前是否可写入数据，或者说socket是否关闭
 *  2.1 TCP 检查buffer是否为满
 *  2.2 UDP 返回1（UDP始终可以发送）
 * 3. 例外情况：未连接的socket，返回0
 */
int socket_write_check(struct Fd* fd) {
	Socket *socket = fd->socket;
	int ret;
	mtx_lock(&socket->lock);

	if (socket->listening) {
		mtx_unlock(&socket->lock);
		ret = 0;
	} else if ((socket->type & 0xf) == SOCK_STREAM) {

		if (socket->self_write_close) {
			ret = 1;
			mtx_unlock(&socket->lock);
			goto out;
		}

		mtx_unlock(&socket->lock);

		// TCP
		Socket *targetSocket = remote_find_peer_socket(socket);

		mtx_lock(&socket->state.state_lock);
		if (socket->state.is_close) {
			ret = 1;
			mtx_unlock(&socket->state.state_lock);
			if (targetSocket != NULL) {
				mtx_unlock(&targetSocket->lock);
			}
			goto out;
		}
		mtx_unlock(&socket->state.state_lock);

		// 对方已关闭
		if (targetSocket == NULL) {
			ret = 1;
			goto out;
		}else if (targetSocket->self_read_close) {
			ret = 1;
			mtx_unlock(&targetSocket->lock);
			goto out;
		}

		ret = (targetSocket->socketWritePos - targetSocket->socketReadPos != SOCKET_BUFFER_SIZE);
		mtx_unlock(&targetSocket->lock);
	} else {
		mtx_unlock(&socket->lock);
		// UDP
		ret = 1;
	}

out:
	return ret;
}

int shutdown(int sockfd, int how) {

	int sfd = cur_proc_fs_struct()->fdList[sockfd];
	warn("%d, %d",sockfd, sfd);
	// assert(fds[sfd].socket != NULL);
	if (sfd < 0 || (sfd >= 0 && fds[sfd].socket == NULL)) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	}

	// if (sfd >= 0 && fds[sfd].type != dev_socket) {
	// 	warn("target fd is not a socket fd, please check\n");
	// 	return -1;
	// }

	Socket *local_socket = fds[sfd].socket;

	if (how == SHUT_RD) {
		mtx_lock(&local_socket->lock);
		local_socket->self_read_close = true;
		wakeup(&local_socket->socketWritePos);
		mtx_unlock(&local_socket->lock);
	} else if (how == SHUT_WR) {
		Socket *target_socket = remote_find_peer_socket(local_socket);
		if (target_socket != NULL) {
			mtx_lock(&target_socket->state.state_lock);
			target_socket->state.opposite_write_close = true;
			mtx_unlock(&target_socket->state.state_lock);
			wakeup(&target_socket->socketReadPos);
			mtx_unlock(&target_socket->lock);
		}
		mtx_lock(&local_socket->lock);
		local_socket->self_write_close= true;
		mtx_unlock(&local_socket->lock);

	} else if (how == SHUT_RDWR) {

		Socket *target_socket = remote_find_peer_socket(local_socket);
		if (target_socket != NULL) {
			mtx_lock(&target_socket->state.state_lock);
			target_socket->state.opposite_write_close = true;
			mtx_unlock(&target_socket->state.state_lock);
			wakeup(&target_socket->socketReadPos);
			mtx_unlock(&target_socket->lock);
		}

		mtx_lock(&local_socket->lock);
		local_socket->self_write_close= true;
		local_socket->self_read_close = true;
		wakeup(&local_socket->socketWritePos);
		mtx_unlock(&local_socket->lock);
	} else {
		return -EINVAL;
	}
	return 0;
}
