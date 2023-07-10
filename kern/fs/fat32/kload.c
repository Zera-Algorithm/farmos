#include <fs/fs.h>
#include <fs/vfs.h>
#include <lock/mutex.h>
#include <lib/printf.h>
#include <lib/error.h>
#include <types.h>
#include <mm/vmm.h>

/**
 * @brief 此文件用于将文件加载到内核
 */

// 内核的一个临时地址，可以用于动态内存分配
#define KERNEL_TEMP 0x600000000ul


extern Pte *kernPd; // 内核页表
mutex_t mtx_file_load;
static Dirent *file = NULL;

fileid_t file_load(const char *path, void **bin, size_t *size) {
	// load时加锁，unload时解锁
	mtx_lock_sleep(&mtx_file_load);

	int _size;
	void *_binary;
	file = getFile(NULL, (char *)path);
	assert(file != NULL);

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
