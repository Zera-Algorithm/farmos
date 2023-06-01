/**
 * 此文件用于解析ELF文件
 */
#include <lib/elf.h>
#include <lib/log.h>
#include <mm/memlayout.h>
#include <mm/vmm.h>
#include <riscv.h>
#include <stddef.h>

/**
 * @brief 从binary解析出ELF头结构体，并检查此结构体是否合法
 * @param binary 二进制数据指针
 * @param size binary的大小
 * @return ElfHeader * 若合法，则返回ELF头结构体指针；否则返回NULL
 */
const ElfHeader *getElfFrom(const void *binary, size_t size) {
	const ElfHeader *ehdr = (const ElfHeader *)binary;
	if (size >= sizeof(ElfHeader) && ehdr->e_magic == ELF_MAGIC) {
		return ehdr;
	}
	return NULL;
}

/**
 * @brief 加载一个ELF格式的二进制文件，把所有段都映射到正确的虚拟地址（即ELF中指定的虚拟地址）
 * @param ph 程序头指针
 * @param binary 段的二进制数据指针，为物理地址
 * @param mapPage 用于将数据映射到虚拟地址的回调函数
 * @param data 辅助数据，用于传入mapPage
 * @return 错误码
 */
int loadElfSegment(ProgramHeader *ph, const void *binary, ElfMapper mapPage, void *data) {
	// va是ELF中指定的该段要被加载到的地址
	u64 va = ph->p_vaddr;
	size_t fileSize = ph->p_filesz; // 文件内数据的长度
	size_t memSize = ph->p_memsz; // 实际加载到内存的数据的长度，memSize始终大于等于fileSize

	// 所有段的权限应至少包括：有效、可读、用户
	u32 perm = PTE_V | PTE_R | PTE_U;

	// 此段是可写的
	if (ph->p_flags & ELF_PROG_FLAG_WRITE) {
		perm |= PTE_W;
	}

	if (ph->p_flags & ELF_PROG_FLAG_EXEC) {
		perm |= PTE_X;
	}

	log(DEFAULT, "load segment to va 0x%016lx, size = 0x%x, perm = 0x%x\n", va, memSize, perm);

	// 1. 映射第一个页
	int r;
	size_t i;
	u64 offset = va - PGROUNDDOWN(va);
	if (offset != 0) {
		if ((r = mapPage(data, va, offset, perm, binary,
				 MIN(fileSize, PAGE_SIZE - offset))) != 0) {
			return r;
		}
	}

	// 2. 把剩余的binary内容（文件内的）加载进内存
	// i = 已写入的长度
	for (i = offset ? MIN(fileSize, PAGE_SIZE - offset) : 0; i < fileSize; i += PAGE_SIZE) {
		if ((r = mapPage(data, va + i, 0, perm, binary + i,
				 MIN(fileSize - i, PAGE_SIZE))) != 0) {
			return r;
		}
	}

	// 3. 当fileSize < memSize时，分配一些空页，以补足缺失的空间
	while (i < memSize) {
		// MOS勘误：这里应作 memSize
		if ((r = mapPage(data, va + i, 0, perm, NULL, MIN(memSize - i, PAGE_SIZE))) != 0) {
			return r;
		}
		i += PAGE_SIZE;
	}
	return 0;
}
