#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <types.h>

#define SYSCALL_ERROR -1

// 系统调用入口
typedef struct trapframe trapframe_t;
void syscall_entry(trapframe_t *tf);

// 内存管理（sys_mem）
err_t sys_map(u64 start, u64 len, u64 perm);
err_t sys_brk(u64 addr);
int sys_madvise(void *addr, size_t length, int advice);
int sys_membarrier(int cmd, int flags);

struct rlimit;

// 进程管理（sys_proc）
void sys_exit(err_t code) __attribute__((noreturn));
err_t sys_exec(u64 path, char **argv, u64 envp);
u64 sys_clone(u64 flags, u64 stack, u64 ptid, u64 tls, u64 ctid);
u64 sys_wait4(u64 pid, u64 status, u64 options);
u64 sys_nanosleep(u64 pTimeSpec);
void sys_sched_yield();
u64 sys_gettid();
u64 sys_getpid();
u64 sys_getppid();
clock_t sys_times(u64 utms);
u64 sys_getuid();
u64 sys_set_tid_address(u64 pTid);
int sys_prlimit64(pid_t pid, int resource, const struct rlimit *new_limit,
		  struct rlimit *old_limit);
pid_t sys_getsid(pid_t pid);
pid_t sys_setsid();
void sys_reboot();

u64 sys_clock_nanosleep(u64 clock_id, u64 flags, u64 request, u64 remain);

struct rusage;
struct sysinfo;

// 系统信息（sys_info）
void sys_uname(u64 upuname);
void sys_gettimeofday(u64 uptv, u64 uptz);
u64 sys_clock_gettime(u64 clockid, u64 tp);
u64 sys_geteuid();
u64 sys_getegid();
u64 sys_getgid();
u64 sys_getpgid();
u64 sys_setpgid(u64 pid, u64 pgid);
int sys_getrusage(int who, struct rusage *p_usage);
int sys_syslog(int priority, const char *format, ...);
int sys_sysinfo(struct sysinfo *info);

struct iovec;
struct statfs;

// 文件系统（sys_fs）
int sys_write(int fd, u64 buf, size_t count);
int sys_read(int fd, u64 buf, size_t count);
size_t sys_pread64(int fd, u64 buf, size_t count, off_t offset);
size_t sys_pwrite64(int fd, u64 buf, size_t count, off_t offset);
int sys_openat(int fd, u64 filename, int flags, mode_t mode);
int sys_close(int fd);
int sys_dup(int fd);
int sys_dup3(int fd_old, int fd_new);
u64 sys_getcwd(u64 buf, int size);
int sys_pipe2(u64 pfd);
int sys_chdir(u64 path);
int sys_mkdirat(int dirFd, u64 path, int mode);
int sys_mount(u64 special, u64 dir, u64 fstype, u64 flags, u64 data);
int sys_umount(u64 special, u64 flags);
int sys_linkat(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags);
int sys_unlinkat(int dirFd, u64 pPath);
int sys_fstat(int fd, u64 pkstat);
int sys_fstatat(int dirFd, u64 pPath, u64 pkstat, int flags);
int sys_getdents64(int fd, u64 buf, int len);
int sys_ioctl(int fd, u64 request, u64 data);
int sys_ppoll(u64 p_fds, int nfds, u64 tmo_p, u64 sigmask);
size_t sys_readv(int fd, const struct iovec *iov, int iovcnt);
size_t sys_writev(int fd, const struct iovec *iov, int iovcnt);
int sys_faccessat(int dirFd, u64 pPath, int mode, int flags);
int sys_fcntl(int fd, int cmd, int arg);
int sys_utimensat(int dirfd, u64 pathname, u64 pTime, int flags);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_renameat2(int olddirfd, u64 oldpath, int newdirfd, u64 newpath, unsigned int flags);
int sys_statfs(u64 ppath, struct statfs *buf);
int sys_ftruncate(int fd, off_t length);
int sys_fsync(int fd);
void sys_sync();
int sys_syncfs(int fd);
int sys_pselect6(int nfds, u64 p_readfds, u64 p_writefds, u64 p_exceptfds, u64 p_timeout, u64 sigmask);
int sys_socketpair(int domain, int type, int protocol, int *fds);
int sys_readlinkat(int dirfd, u64 pathname, u64 buf, size_t bufsiz);

// 信号（sys_signal）
int sys_sigaction(int signum, u64 act, u64 oldact, int sigset_size);
int sys_sigreturn();
int sys_sigprocmask(int how, u64 set, u64 oldset, size_t sigsetsize);
int sys_tkill(int tid, int sig);
int sys_kill(int pid, int sig);
int sys_sigtimedwait(u64 usigset, u64 uinfo, u64 utimeout);
int sys_setitimer(int which, u64 new_value, u64 old_value);
int sys_getitimer(int which, u64 curr_value);
int sys_sigsuspend(u64 usigset);

// MMAP(sys_mmap)
void *sys_mmap(u64 start, size_t len, int prot, int flags, int fd, off_t off);
err_t sys_msync(u64 addr, size_t length, int flags);
err_t sys_unmap(u64 start, u64 len);
err_t sys_mprotect(u64 addr, size_t len, int prot);

typedef struct SocketAddr SocketAddr;

// socket
int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const SocketAddr *sockectaddr, socklen_t addrlen);
int sys_listen(int sockfd, int backlog);
int sys_connect(int sockfd, const SocketAddr *addr, socklen_t addrlen);
int sys_accept(int sockfd, SocketAddr *addr);
int sys_recvfrom(int sockfd, void *buffer, size_t len, int flgas, SocketAddr * src_addr, socklen_t addrlen);
int sys_sendto(int sockfd, const void * buffer, size_t len, int flags, const SocketAddr * dst_addr, socklen_t addrlen);
int sys_getsocketname(int sockfd, SocketAddr * addr, socklen_t addrlen);
int sys_getsockopt(int sockfd, int lever, int optname, void * optval, socklen_t * optlen);
int sys_setsockopt(int sockfd, int lever, int optname, const void * optval, socklen_t optlen);
int sys_getpeername(int sockfd, SocketAddr * addr, socklen_t* addrlen);
int sys_shutdown(int sockfd, int how);

// 调度（sys_sched）
u64 sys_sched_getaffinity();
u64 sys_sched_setaffinity();
u64 sys_sched_getscheduler();
u64 sys_sched_setscheduler();
u64 sys_sched_getparam();

// ipc
int sys_shmget(u64 key, u64 size, int shmflg);
void *sys_shmat(int shmid, u64 shmaddr, int shmflg);
int sys_shmctl(int shmid, int cmd, u64 arg_buf);

// Futex(sys_futex)
int sys_futex(u64 uaddr, u64 futex_op, u64 val, u64 val2, u64 uaddr2, u64 val3);
u64 sys_get_robust_list();
#endif // !_SYSCALL_H
