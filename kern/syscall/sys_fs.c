#include <fs/fd.h>
#include <fs/file.h>
#include <fs/kload.h>
#include <fs/thread_fs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/syscall.h>
#include <sys/syscall_fs.h>

int sys_write(int fd, u64 buf, size_t count) {
	// todo
	return write(fd, buf, count);
}

int sys_read(int fd, u64 buf, size_t count) {
	return read(fd, buf, count);
}

int sys_openat(int fd, u64 filename, int flags, mode_t mode) {
	return openat(fd, filename, flags, mode);
}

/**
 * @brief 将文件映射到进程的虚拟内存空间
 * @note 如果start == 0，则由内核指定虚拟地址
 * @todo 实现匿名映射（由flags的MAP_ANONYMOUS标志决定），即不映射到文件
 */
// TODO: 需要考虑flags的其他字段，但暂未考虑
// TODO: 实现将映射地址区域与文件结合，实现msync同步内存到文件
void *sys_mmap(u64 start, size_t len, int prot, int flags, int fd, off_t off) {
	int r = 0, perm = 0;
	Dirent *file;
	thread_t *thread = cpu_this()->cpu_running;

	// 1. 当start为0时，由内核指定用户的虚拟地址，记录到start中
	if (start == 0) {
		// Note: 指定的固定地址在MMAP_START到MMAP_END之间
		start = thread->td_fs_struct.mmap_addr;
		u64 new_addr = PGROUNDUP(start + len);

		if (new_addr > MMAP_END) {
			warn("no more free mmap space to alloc!");
			return MAP_FAILED;
		}

		thread->td_fs_struct.mmap_addr = new_addr;
	}

	// 2. 指定权限位
	perm = PTE_U;
	if (prot & PROT_EXEC) {
		perm |= PTE_X;
	}
	if (prot & PROT_READ) {
		perm |= PTE_R;
	}
	if (prot & PROT_WRITE) {
		perm |= PTE_W;
	}

	if (flags & MAP_ANONYMOUS) {
		// 匿名映射
		panic_on(sys_map(start, len, perm));
		return (void *)start;
	} else {
		// 文件映射
		// 3. 通过fd获取文件
		r = getDirentByFd(fd, &file, NULL);
		if (r < 0) {
			warn("get fd(%d) error!\n", fd);
			return MAP_FAILED;
		}

		return file_map(cpu_this()->cpu_running, file, start, len, perm, off);
	}
}

int sys_fstat(int fd, u64 pkstat) {
	return fileStatFd(fd, pkstat);
}
