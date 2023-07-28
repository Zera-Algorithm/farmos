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

int sys_recvfrom(int sockfd, void *buffer, size_t len, int flgas, SocketAddr * src_addr, socklen_t addrlen) {
	return recvfrom(sockfd, buffer, len, flgas, src_addr, addrlen);
}

int sys_sendto(int sockfd, const void * buffer, size_t len, int flags, const SocketAddr * dst_addr, socklen_t addrlen) {
	return sendto(sockfd, buffer, len, flags, dst_addr, addrlen);
}

int sys_getsocketname(int sockfd, SocketAddr * addr, socklen_t addrlen) {
	return getsocketname(sockfd, addr, addrlen);
}

int sys_getsockopt(int sockfd, int lever, int optname, void * optval, socklen_t * optlen) {
	return getsockopt(sockfd, lever, optname, optval, optlen);
}

int sys_setsockopt(int sockfd, int lever, int optname, const void * optval, socklen_t optlen) {
	return setsockopt(sockfd, lever, optname, optval, optlen);
}
