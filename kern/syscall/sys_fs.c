#include <fs/fd.h>
#include <fs/file.h>
#include <fs/kload.h>
#include <fs/pipe.h>
#include <fs/thread_fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/syscall.h>
#include <sys/syscall_fs.h>

int sys_write(int fd, u64 buf, size_t count) {
	return write(fd, buf, count);
}

int sys_read(int fd, u64 buf, size_t count) {
	return read(fd, buf, count);
}

int sys_openat(int fd, u64 filename, int flags, mode_t mode) {
	return openat(fd, filename, flags, mode);
}

int sys_close(int fd) {
	return closeFd(fd);
}

int sys_dup(int fd) {
	return dup(fd);
}

int sys_dup3(int fd_old, int fd_new) {
	return dup3(fd_old, fd_new);
}

int sys_getcwd(u64 buf, int size) {
	void *kptr = cpu_this()->cpu_running->td_fs_struct.cwd;
	copyOut(buf, kptr, MIN(256, size));
	return buf;
}

int sys_pipe2(u64 pfd) {
	int fd[2];
	int ret = pipe(fd);
	if (ret < 0) {
		return ret;
	} else {
		copyOut(pfd, fd, sizeof(fd));
		return ret;
	}
}

int sys_chdir(u64 path) {
	char kbuf[MAX_NAME_LEN];
	copyInStr(path, kbuf, MAX_NAME_LEN);
	thread_t *thread = cpu_this()->cpu_running;

	if (kbuf[0] == '/') {
		// 绝对路径
		strncpy(thread->td_fs_struct.cwd, kbuf, MAX_NAME_LEN);
		assert(strlen(thread->td_fs_struct.cwd) + 3 < MAX_NAME_LEN);
		strcat(thread->td_fs_struct.cwd, "/"); // 保证cwd是一个目录
	} else {
		// 相对路径
		// 保证操作之前cwd以"/"结尾
		assert(strlen(thread->td_fs_struct.cwd) + strlen(kbuf) + 3 < MAX_NAME_LEN);
		strcat(thread->td_fs_struct.cwd, kbuf);
		strcat(thread->td_fs_struct.cwd, "/");
	}
	return 0;
}

int sys_mkdirat(int dirFd, u64 path, int mode) {
	return makeDirAtFd(dirFd, path, mode);
}

int sys_mount(u64 special, u64 dir, u64 fstype, u64 flags, u64 data) {
	char specialStr[MAX_NAME_LEN];
	char dirPath[MAX_NAME_LEN];

	// 1. 将special和dir加载到字符串数组中
	copyInStr(special, specialStr, MAX_NAME_LEN);
	copyInStr(dir, dirPath, MAX_NAME_LEN);

	// 2. 计算cwd，如果dir不是绝对路径，则是相对于cwd
	Dirent *cwd = get_cwd_dirent(&(cpu_this()->cpu_running->td_fs_struct));

	// 3. 挂载
	return mount_fs(specialStr, cwd, dirPath);
}

int sys_umount(u64 special, u64 flags) {
	char specialStr[MAX_NAME_LEN];

	// 1. 将special和dir加载到字符串数组中
	copyInStr(special, specialStr, MAX_NAME_LEN);

	// 2. 计算cwd，如果dir不是绝对路径，则是相对于cwd
	Dirent *cwd = get_cwd_dirent(&(cpu_this()->cpu_running->td_fs_struct));

	// 3. 解除挂载
	return umount_fs(specialStr, cwd);
}

int sys_linkat(int oldFd, u64 pOldPath, int newFd, u64 pNewPath, int flags) {
	return linkAtFd(oldFd, pOldPath, newFd, pNewPath, flags);
}

int sys_unlinkat(int dirFd, u64 pPath) {
	return unLinkAtFd(dirFd, pPath);
}

int sys_getdents64(int fd, u64 buf, int len) {
	return getdents64(fd, buf, len);
}

/**
 * @brief 将文件映射到进程的虚拟内存空间
 * @note 如果start == 0，则由内核指定虚拟地址
 * @todo 实现匿名映射（由flags的MAP_ANONYMOUS标志决定），即不映射到文件
 */
// TODO: 需要考虑flags的其他字段，但暂未考虑
// TODO: 实现将映射地址区域与文件结合，实现msync同步内存到文件
void *sys_mmap(u64 start, size_t len, int prot, int flags, int fd, off_t off) {
	panic("sys_mmap not implemented!");
	// int r = 0, perm = 0;
	// Dirent *file;
	// thread_t *thread = cpu_this()->cpu_running;

	// // 1. 当start为0时，由内核指定用户的虚拟地址，记录到start中
	// if (start == 0) {
	// 	// Note: 指定的固定地址在MMAP_START到MMAP_END之间
	// 	start = thread->td_fs_struct.mmap_addr;
	// 	u64 new_addr = PGROUNDUP(start + len);

	// 	if (new_addr > MMAP_END) {
	// 		warn("no more free mmap space to alloc!");
	// 		return MAP_FAILED;
	// 	}

	// 	thread->td_fs_struct.mmap_addr = new_addr;
	// }

	// // 2. 指定权限位
	// perm = PTE_U;
	// if (prot & PROT_EXEC) {
	// 	perm |= PTE_X;
	// }
	// if (prot & PROT_READ) {
	// 	perm |= PTE_R;
	// }
	// if (prot & PROT_WRITE) {
	// 	perm |= PTE_W;
	// }

	// if (flags & MAP_ANONYMOUS) {
	// 	// 匿名映射
	// 	panic_on(sys_map(start, len, perm));
	// 	return (void *)start;
	// } else {
	// 	// 文件映射
	// 	// 3. 通过fd获取文件
	// 	r = getDirentByFd(fd, &file, NULL);
	// 	if (r < 0) {
	// 		warn("get fd(%d) error!\n", fd);
	// 		return MAP_FAILED;
	// 	}

	// 	return file_map(cpu_this()->cpu_running, file, start, len, perm, off);
	// }
}

int sys_fstat(int fd, u64 pkstat) {
	return fileStatFd(fd, pkstat);
}
