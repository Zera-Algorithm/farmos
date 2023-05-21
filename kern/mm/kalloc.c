#include <dev/dtb.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <riscv.h>

/**
 * @brief 本文件负责内核未建立起页表映射结构时的内存分配
 */

// 标识当前起始的第一块空闲内存的位置
extern char end[];
void *freeMem;
extern struct MemInfo memInfo;

void kinit() {
	log("end = 0x%08lx\n", end);
	freeMem = (void *)PGROUNDUP((uint64)end);
}

/**
 * @attention 只能在内核未建立页表机制之前使用！
 * @brief 分配指定长度的内存，最后会把freeMem对齐到一页的整数倍位置
 * @brief 保证分配的指针的位置是4K的整数倍
 */
void *kalloc(uint64 size) {
	void *ret = freeMem;
	memset(ret, 0, size);
	freeMem += size;
	freeMem = (void *)PGROUNDUP((uint64)freeMem);
	return ret;
}
