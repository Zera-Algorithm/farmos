#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/error.h>
#include <lib/printf.h>
#include <lock/mutex.h>
#include <mm/vmm.h>
#include <types.h>
#include <lib/elf.h>
#include <proc/thread.h>

/**
 * @brief 此文件用于将文件加载到内核
 */

// 内核的一个临时地址，可以用于动态内存分配
#define KERNEL_TEMP 0x600000000ul

extern Pte *kernPd; // 内核页表
mutex_t mtx_file_load;
static Dirent *file = NULL;

static fileid_t file_load_by_dirent(Dirent *dirent, void **bin, size_t *size) {
	mtx_lock_sleep(&mtx_file_load);

	int _size;
	void *_binary;

	*size = _size = file->file_size;
	*bin = _binary = (void *)KERNEL_TEMP;

	// 1. 分配足够的页
	int npage = (_size) % PAGE_SIZE == 0 ? (_size / PAGE_SIZE) : (_size / PAGE_SIZE + 1);
	for (int i = 0; i < npage; i++) {
		u64 pa = vmAlloc();
		u64 va = ((u64)_binary) + i * PAGE_SIZE;
		panic_on(ptMap(kernPd, va, pa, PTE_R | PTE_W));
	}

	// 2. 读取文件
	file_read(file, 0, (u64)_binary, 0, _size);
	return 0;
}


fileid_t file_load(const char *path, void **bin, size_t *size) {
	// load时加锁，unload时解锁
	file = getFile(NULL, (char *)path);
	assert(file != NULL);

	return file_load_by_dirent(file, bin, size);
}

void file_unload(fileid_t fileid) {
	assert(file != NULL);
	int _size = file->file_size;
	int npage = (_size) % PAGE_SIZE == 0 ? (_size / PAGE_SIZE) : (_size / PAGE_SIZE + 1);

	// 1. 释放文件
	file_close(file);

	// 2. 解除页面映射
	for (int i = 0; i < npage; i++) {
		u64 va = KERNEL_TEMP + i * PAGE_SIZE;
		panic_on(ptUnmap(kernPd, va));
	}

	mtx_unlock_sleep(&mtx_file_load);
}

/**
 * @brief 读取一个文件内容到内存，并映射其内容到某个页表
 * @param pgDir 要建立映射的页表
 * @param startVa 开始映射的虚拟地址位置
 * @param len 要读取文件的长度
 * @param perm 映射的权限位
 * @param offset 开始读取的文件偏移
 * @return 如果映射成功，返回映射位置的指针，否则返回-1
 */
void *file_map(thread_t *proc, Dirent *file, u64 va, size_t len, int perm, int fileOffset) {
	int size;
	void *binary;

	// 1. 将文件加载到内核中，位置位于binary，内容大小为size
	fileid_t fileid = file_load_by_dirent(file, &binary, &size);

	// 2. 判断fileOffset是否合法
	if (fileOffset >= size) {
		warn("fileOffset %d > fileSize %d!\n", fileOffset, size);
		return (void *)-1;
	}

	// 3. 约束len
	len = MIN(len, size - fileOffset);
	binary += fileOffset; // 将binary移动到可以立即开始读取的位置

	// 4.1 映射第一个页
	int r;
	size_t i;
	u64 offset = va - PGROUNDDOWN(va);
	if (offset != 0) {
		if ((r = loadDataMapper(proc->td_pt, va, offset, perm, binary,
					MIN(len, PAGE_SIZE - offset))) != 0) {
			warn("map error! r = %d\n", r);
			return (void *)-1;
		}
	}

	// 4.2 把剩余的binary内容（文件内的）加载进内存
	// i = 已写入的长度
	for (i = offset ? MIN(len, PAGE_SIZE - offset) : 0; i < len; i += PAGE_SIZE) {
		if ((r = loadDataMapper(proc->td_pt, va + i, 0, perm, binary + i,
					MIN(len - i, PAGE_SIZE))) != 0) {
			warn("map error! r = %d\n", r);
			return (void *)-1;
		}
	}

	// 5. 释放内核加载的文件
	file_unload(fileid);
	return (void *)va;
}
