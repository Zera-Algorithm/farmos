#include <fs/fd.h>
#include <fs/socket.h>
#include <mm/vmm.h>
#include <proc/interface.h>

static uint socketBitMap[SOCKET_COUNT / 32] = {0};
Socket sockets[SOCKET_COUNT];

struct mutex mtx_socketMap;

int socketAlloc() {
	// TODO 对socketBitMap加锁，使得只有一个进程能够进入Alloc函数
	for (int i = 0; i < SOCKET_COUNT; i++) {
		for (i = 0; i < FDNUM; i++) {
			int index = i >> 5;
			int inner = i & 31;
			if ((socketBitMap[index] & (1 << inner)) == 0) {
				socketBitMap[index] |= 1 << inner;
				mtx_init(&sockets[i].lock, "socket_lock", 1, MTX_SLEEP);
				return i;
			}
		}
	}
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

	int sfd = fdAlloc();
	if (sfd < 0) {
		warn("All fd in kernel is used, please check\n");
		socketFree(socketNum);
		return -1;
	}

	// fds list加锁
	mtx_lock(&fds[sfd].lock);
	fds[sfd].dirent = NULL;
	fds[sfd].pipe = NULL;
	fds[sfd].type = dev_socket;
	fds[sfd].flags = O_RDWR;
	fds[sfd].offset = 0;
	// TODO TODO fds[sfd].fd_dev = &fd_dev_pipe;
	fds[sfd].socket = socket;
	fds[sfd].socketAddr = kvmAlloc();
	mtx_unlock(&fds[sfd].lock);
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

	if (socket->addr.family == sockectaddr->family)
		;
	// assert(sa->addr == 0 || (sa->addr >> 24) == 127);
	socket->addr.addr = sockectaddr->addr;
	socket->addr.port = sockectaddr->port;

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

	// TODO 如何体现无法在一个已连接的 socket（即已经成功执行 connect()的 socket 或由
	// accept()调用返回的 socket）上执行 listen()
	return 0;
}

int connect(int sockfd, const SocketAddr *addr, socklen_t addrlen) {

	int sfd = cur_proc_fs_struct()->fdList[sockfd];

	if (sfd >= 0 && fds[sfd].type != dev_socket) {
		warn("target fd is not a socket fd, please check\n");
		return -1;
	} // 检查Fd类型是否匹配

	Socket *local_socket = fds[sfd].socket;

	local_socket->addr = gen_local_socket_addr();
	local_socket->target_addr = *addr;

	Socket *target_socket = find_listening_socket(addr);
	if (target_socket == NULL) {
		printf("server socket doesn't exists or isn't listening");
		return -1;
	}

	mtx_lock(&local_socket->lock);
	// 得到服务端socket的锁

	if (target_socket->waiting_t - target_socket->waiting_h == PENDING_COUNT) {
		return -1; // 达到最高限制
	}

	target_socket->waiting_queue[(target_socket->waiting_t++) % PENDING_COUNT] =
	    local_socket->addr;

	// TODO 释放服务端的锁，睡眠，等待服务端唤醒客户端

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
		// sleep(&socket->lock);
		// 等到客户端有请求时，唤醒服务端，并且被唤醒后，此时服务端拥有服务端socket的锁
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

	// TODO 释放服务端socket锁，唤醒newsocket对应的客户端

	return newUerFd;
}

static SocketAddr gen_local_socket_addr() {
	static int local_addr = (127 << 24) + 1;
	static int local_port = 10000;
	SocketAddr addr;
	addr.family = 2;
	addr.addr = local_addr;
	addr.port = local_port++;
	return addr;
}

static Socket *find_listening_socket(const SocketAddr *addr) {
	for (int i = 0; i < SOCKET_COUNT; ++i) {
		if (sockets[i].used && sockets[i].addr.family == addr->family &&
		    sockets[i].addr.port == addr->port && sockets[i].listening) {
			return &sockets[i];
		}
	}
	return NULL;
}

void socketFree(int socketNum) {

	mtx_lock(&mtx_socketMap);
	//  对socketBitMap加锁，使得只有一个进程能够进入Alloc函数

	assert(socketNum >= 0 && socketNum < SOCKET_COUNT);
	int index = socketNum >> 5;
	int inner = socketNum & 31;
	socketBitMap[index] &= ~(1 << inner);
	mtx_unlock(&mtx_socketMap);

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

	memset(&socket->addr, 0, sizeof(SocketAddr));
	memset(&socket->target_addr, 0, sizeof(SocketAddr));
	memset(socket->waiting_queue, 0, (sizeof(SocketAddr) * PENDING_COUNT));
	mtx_unlock(&socket->lock);
}
