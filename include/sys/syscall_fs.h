#ifndef _SYSCALL_FS_H
#define _SYSCALL_FS_H

// Mmap 的一些宏定义
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0X01000000
#define PROT_GROWSUP 0X02000000

#define MAP_FILE 0
#define MAP_SHARED 0x01	 /* Share changes.  */
#define MAP_PRIVATE 0X02 /* Changes are private.  */
#define MAP_FAILED ((void *)-1)

// MMAP的标志位flags
// 取自/usr/include/bits/mman-linux.h
/* Other flags.  */
#define MAP_FIXED 0x10	   /* Interpret addr exactly.  */
#define MAP_ANONYMOUS 0x20 /* Don't use a file.  */

typedef struct iovec {
	void *iov_base; /* Starting address.  */
	size_t iov_len; /* Number of bytes to transfer.  */
} iovec_t;

// 用于lssek
#define SEEK_SET 0  /* Seek from beginning of file.  */
#define SEEK_CUR 1  /* Seek from current position.  */
#define SEEK_END 2  /* Seek from end of file.  */
#define SEEK_DATA 3 /* Seek to next data.  */
#define SEEK_HOLE 4 /* Seek to next hole.  */

// 用于fstatat的flags
#define AT_SYMLINK_NOFOLLOW 0x100 /* Do not follow symbolic links.  */
#define AT_REMOVEDIR                                                                               \
	0x200			/* Remove directory instead of                                     \
				   unlinking file.  */
#define AT_SYMLINK_FOLLOW 0x400 /* Follow symbolic links.  */
#define AT_NO_AUTOMOUNT                                                                            \
	0x800		     /* Suppress terminal automount                                        \
				traversal.  */
#define AT_EMPTY_PATH 0x1000 /* Allow empty relative pathname.  */

// 用于ppoll
struct pollfd {
	int fd;	       /* file descriptor */
	short events;  /* requested events */
	short revents; /* returned events */
};

// 取自bits/poll.h

#define POLLIN 0x001  /* There is data to read.  */
#define POLLPRI 0x002 /* There is urgent data to read.  */
#define POLLOUT 0x004 /* Writing now will not block.  */

/* These values are defined in XPG4.2.  */
#define POLLRDNORM 0x040 /* Normal data may be read.  */
#define POLLRDBAND 0x080 /* Priority data may be read.  */
#define POLLWRNORM 0x100 /* Writing now will not block.  */
#define POLLWRBAND 0x200 /* Priority data may be written.  */

/* These are extensions for Linux.  */
#define POLLMSG 0x400
#define POLLREMOVE 0x1000
#define POLLRDHUP 0x2000

/* Event types always implicitly polled for.  These bits need not be set in
   `events', but they will appear in `revents' to indicate the status of
   the file descriptor.  */
#define POLLERR 0x008  /* Error condition.  */
#define POLLHUP 0x010  /* Hung up. 如管道或Socket中，对端关闭了连接 */
#define POLLNVAL 0x020 /* Invalid polling request. fd未打开 */

// fcntl的cmd参数取值
#define FCNTL_GETFD 1
#define FCNTL_SETFD 2
#define FCNTL_GET_FILE_STATUS 3
#define FCNTL_DUPFD_CLOEXEC 1030
#define FCNTL_SETFL 4 /* Set file status flags.  */

// for getfd and setfd
#define FD_CLOEXEC 1

#define __FSID_T_TYPE                                                                              \
	struct {                                                                                   \
		int __val[2];                                                                      \
	}

struct statfs {
	i64 f_type;
	i64 f_bsize;
	u64 f_blocks;
	u64 f_bfree;
	u64 f_bavail;
	u64 f_files;
	u64 f_ffree;
	struct {
		int val[2];
	} f_fsid;
	i64 f_namelen;
	i64 f_frsize;
	i64 f_flags;
	i64 f_spare[4]; // 保留位
};

// used by pselect
#define FD_SETSIZE 1024

typedef struct fd_set {
	u64 fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;

#define FD_ISSET(fd, set) ((set)->fds_bits[(fd) / (8 * sizeof(long))] & (1UL << ((fd) % (8 * sizeof(long)))))

static inline void FD_SET(int fd, fd_set *set) {
	set->fds_bits[fd / (8 * sizeof(long))] |= (1UL << (fd % (8 * sizeof(long))));
}

static inline void FD_CLR(int fd, fd_set *set) {
	set->fds_bits[fd / (8 * sizeof(long))] &= ~(1UL << (fd % (8 * sizeof(long))));
}

#define FD_SET_FOREACH(fd, set)                                                                    \
	for (fd = 0; fd < FD_SETSIZE; fd++)                                                       \
		if (FD_ISSET(fd, set))

#endif
