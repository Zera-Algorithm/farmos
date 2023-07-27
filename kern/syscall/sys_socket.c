#include <fs/socket.h>

int sys_socket(int domain, int type, int protocol) {
	return socket(domain, type, protocol);
}

int sys_bind(int sockfd, const SocketAddr *sockectaddr, socklen_t addrlen) {
	return bind(sockfd, sockectaddr, addrlen);
}

int sys_listen(int sockfd, int backlog) {
	return listen(sockfd, backlog);
}

int sys_connect(int sockfd, const SocketAddr *addr, socklen_t addrlen) {
	return connect(sockfd, addr, addrlen);
}

int sys_accept(int sockfd, SocketAddr *addr, socklen_t * addrlen) {
	return accept(sockfd, addr, addrlen);
}