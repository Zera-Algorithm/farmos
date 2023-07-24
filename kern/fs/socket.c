#include <fs/fd.h>
#include <fs/fd_device.h>
#include <fs/socket.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <proc/interface.h>

static uint socket_bitmap[SOCKET_COUNT / 32] = {0};
Socket sockets[SOCKET_COUNT];

struct mutex mtx_socketmap;

static int fd_socket_read(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_socket_write(struct Fd *fd, u64 buf, u64 n, u64 offset);
static int fd_socket_close(struct Fd *fd);
static int fd_socket_stat(struct Fd *fd, u64 pkStat);

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
	// TODO 不是很理解这里 sys_socketable的意义，请review一下
}

int socketAlloc() {
	int i;
	// TODO 对socketBitMap加锁，使得只有一个进程能够进入Alloc函数
	mtx_lock(&mtx_socketmap);
	for (i = 0; i < SOCKET_COUNT; i++) {
		int index = i >> 5;
		int inner = i & 31;
		if ((socket_bitmap[index] & (1 << inner)) == 0) {
			socket_bitmap[index] |= 1 << inner;
			mtx_init(&sockets[i].lock, "socket_lock", 1, MTX_SPIN);
			mtx_init(&sockets[i].state.state_lock, "socket_state_lock", 1, MTX_SPIN);
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
	socket->bufferAddr = kvmAlloc();

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
	fds[sfd].flags = O_RDWR;
	fds[sfd].offset = 0;
	fds[sfd].fd_dev = &fd_dev_socket;
	fds[sfd].socket = socket;
	mtx_unlock_sleep(&fds[sfd].lock);
	// TODO 讨论这里对fds[sfd]加锁是否有必要

	cur_proc_fs_struct()->fdList[usfd] = sfd;

	mtx_unlock(&socket->lock);
	// 释放socket结构体的锁

	return usfd;
}

int bind(int sockfd, const SocketAddr *sockectaddr, socklen_t addrlen) {
	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	} // 检查Fd类型是否匹配

	Socket *socket = fds[sfd].socket;

	mtx_lock(&socket->lock);

	if (socket->addr.family == sockectaddr->family) {
		socket->addr.addr = sockectaddr->addr;
		socket->addr.port = sockectaddr->port;
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

	// TODO 对socket进行加锁
	mtx_lock(&socket->lock);
	socket->listening = 1;
	mtx_unlock(&socket->lock);

	// TODO 如何体现无法在一个已连接的 socket（即已经成功执行 connect的 socket
	// 或由accept调用返回的 socket）上执行 listen
	return 0;
}

int connect(int sockfd, const SocketAddr *addr, socklen_t addrlen) {

	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	} // 检查Fd类型是否匹配

	Socket *local_socket = fds[sfd].socket;

	mtx_lock(&local_socket->lock);
	local_socket->addr = gen_local_socket_addr();
	local_socket->target_addr = *addr;
	mtx_unlock(&local_socket->lock);

	Socket *target_socket = find_listening_socket(addr);
	if (target_socket == NULL) {
		printf("server socket doesn't exists or isn't listening");
		return -1;
	}

	mtx_lock(&target_socket->lock);

	if (target_socket->waiting_t - target_socket->waiting_h == PENDING_COUNT) {
		return -1; // 达到最高限制
	}

	target_socket->waiting_queue[(target_socket->waiting_t++) % PENDING_COUNT] =
	    local_socket->addr;

	int pos = (target_socket->waiting_t - 1 + PENDING_COUNT) % PENDING_COUNT;

	//  释放服务端target_socket的锁，客户端进入睡眠，等待服务端唤醒客户端
	sleep(&target_socket->lock, &target_socket->waiting_queue[pos],
	      "connecting... waiting for target socket to accept\n");

	// 尝试唤醒服务端，以等待队列指针作为chan
	wakeup(target_socket->waiting_queue);
	mtx_unlock(&target_socket->lock);

	return 0;
}

int accept(int sockfd, SocketAddr *addr) {

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
		sleep(&local_socket->lock, local_socket->waiting_queue,
		      "waiting for socket to enter waiting queue...\n");
	}

	// Socket *addr = socket->waiting_queue[(socket->waiting_h++) % PENDING_COUNT];

	newUerFd = socket(local_socket->addr.family, local_socket->type, 0);
	if (newUerFd < 0) {
		return -1;
	}

	Socket *newSocket = fds[cur_proc_fs_struct()->fdList[sockfd]].socket;

	newSocket->addr = local_socket->addr;
	newSocket->target_addr =
	    local_socket->waiting_queue[(local_socket->waiting_h++) % PENDING_COUNT];

	//  释放服务端socket锁，唤醒newsocket对应的客户端
	int pos = (local_socket->waiting_h - 1 + PENDING_COUNT) % PENDING_COUNT;
	wakeup(&local_socket->waiting_queue[pos]);
	mtx_unlock(&local_socket->lock);

	return newUerFd;
}

static SocketAddr gen_local_socket_addr() {
	static int local_addr = (127 << 24) + 1;
	static int local_port = 10000;
	SocketAddr addr;
	addr.family = AF_UNIX;
	addr.addr = local_addr;
	addr.port = local_port++;
	return addr;
}

static Socket *find_listening_socket(const SocketAddr *addr) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		mtx_lock(&sockets[i].lock);
		if (sockets[i].used && sockets[i].addr.family == addr->family &&
		    sockets[i].addr.addr == addr->addr && sockets[i].addr.port == addr->port &&
		    sockets[i].listening) {
			mtx_unlock(&sockets[i].lock);
			return &sockets[i];
		}
		mtx_unlock(&sockets[i].lock);
	}
	return NULL;
	// TODO 检查加锁的合理性
}

static Socket *remote_find_peer_socket(const Socket *local_socket) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		mtx_lock(&sockets[i].lock);
		if (sockets[i].used && sockets[i].addr.family == local_socket->target_addr.family &&
		    sockets[i].addr.port == local_socket->target_addr.port &&
		    sockets[i].addr.addr == local_socket->target_addr.addr &&
		    sockets[i].target_addr.port == local_socket->addr.port &&
		    sockets[i].target_addr.addr == local_socket->addr.addr) {
			mtx_unlock(&sockets[i].lock);
			return &sockets[i];
		}
		mtx_unlock(&sockets[i].lock);
	}
	return NULL;
}

static int fd_socket_read(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	int i;
	char ch;
	char *readPos;
	Socket *localSocket = fd->socket;
	mtx_lock(&localSocket->lock);
	// 读时，对自身socket进行加自旋锁

	while (localSocket->socketReadPos == localSocket->socketWritePos) {
		mtx_lock(&localSocket->state.state_lock);
		if (!localSocket->state.is_close) {
			mtx_unlock(&localSocket->state.state_lock);

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
		copyOut((buf + i), &ch, 1);
		localSocket->socketReadPos++;
	}
	fd->offset += i;

	wakeup(&localSocket->socketWritePos);
	mtx_unlock(&localSocket->lock);

	return i;
}

static int fd_socket_write(struct Fd *fd, u64 buf, u64 n, u64 offset) {
	int i = 0;
	char ch;
	char *writePos;
	Socket *localSocket = fd->socket;
	Socket *targetSocket = remote_find_peer_socket(localSocket);

	mtx_lock(&targetSocket->lock);

	while (i < n) {
		mtx_lock(
		    &localSocket->state
			 .state_lock); // 获得自身socket的状态锁，从而来获得targetSocket是否关闭的状态
		if (targetSocket->socketWritePos - targetSocket->socketReadPos == PAGE_SIZE) {
			if (localSocket->state.is_close /* 对面socket进程已结束*/) {
				mtx_unlock(&localSocket->state.state_lock);
				mtx_unlock(&targetSocket->lock);
				warn("socket writer can\'t write more.\n");
				return -1;
			} else {
				mtx_unlock(&localSocket->state.state_lock);

				wakeup(
				    &targetSocket->socketReadPos); // TODO 讨论这一wakeup是否必要，
				mtx_unlock_sleep(&fd->lock);
				sleep(&targetSocket->socketWritePos, &targetSocket->lock,
				      "wait another socket to  read.\n");
				mtx_lock_sleep(&fd->lock);
			}
		} else {
			copyIn((buf + i), &ch, 1);
			writePos = (char *)(targetSocket->bufferAddr +
					    ((targetSocket->socketWritePos) % PAGE_SIZE));
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

	mtx_lock(&mtx_socketmap);
	//  对socketBitMap加锁，使得只有一个进程能够进入Alloc函数

	assert(socketNum >= 0 && socketNum < SOCKET_COUNT);
	int index = socketNum >> 5;
	int inner = socketNum & 31;
	socket_bitmap[index] &= ~(1 << inner);
	mtx_unlock(&mtx_socketmap);

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
		kvmFree(socket->bufferAddr);
	}

	memset(&socket->addr, 0, sizeof(SocketAddr));
	memset(&socket->target_addr, 0, sizeof(SocketAddr));
	memset(socket->waiting_queue, 0, (sizeof(SocketAddr) * PENDING_COUNT));
	mtx_unlock(&socket->lock);

	mtx_lock(&socket->state.state_lock);
	socket->state.is_close = false;
	mtx_unlock(&socket->state.state_lock);
}

static int fd_socket_close(struct Fd *fd) {
	Socket *localSocket = fd->socket;
	Socket *targetSocket = remote_find_peer_socket(localSocket);
	if (targetSocket != NULL) {
		mtx_lock(&targetSocket->state.state_lock);
		targetSocket->state.is_close = true;
		mtx_unlock(&targetSocket->state.state_lock);
	}
	int socketNum = localSocket - sockets;
	socketFree(socketNum);

	return 0;
}

static int fd_socket_stat(struct Fd *fd, u64 pkStat) {
	return 0;
}