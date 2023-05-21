#include <dev/dtb.h>
#include <lib/string.h>
#include <mm/kalloc.h>
#include <mm/memlayout.h>
#include <mm/memory.h>
#include <riscv.h>

extern char end_text[], trampoline[];

// 内核页表
Pte *kernelPageTable;
extern struct MemInfo memInfo;

// pages数组，其大小等于实际的物理内存页数npages
uint64 npages;
Page *pages;

// 空闲链表
PageList pageFreeList;

static void initDataStructure();

/**
 * @brief 为riscv的单个hart开启分页功能
 */
void enablePagingHart() {
	// // 等待之前对页表的写操作结束
	// sfence_vma();

	w_satp(MAKE_SATP(kernelPageTable));

	// 刷新TLB（单核）
	sfence_vma();
}

/**
 * @brief 为内核映射连续的一段内存地址
 * @note 调用者应当保证va, pa, size页对齐
 * @author zrp
 */
static void kernelPageMap(Pte *pageDir, uint64 va, uint64 pa, uint64 size, uint64 perm) {
	extern void kernelPageInsert(Pte * pageDir, uint64 va, uint64 pa, uint64 perm);
	for (uint64 i = 0; i < size; i += PAGE_SIZE) {
		kernelPageInsert(pageDir, va + i, pa + i, perm);
	}
}

/**
 * @brief 为内核的所有内存建立映射
 * @returns 内核页表指针
 */
Pte *initKvmPageTable() {
	// 分配一个一级页表页
	Pte *kPageTable = (Pte *)kalloc(PGSIZE);
	// memset((void *)kPageTable, 0, PGSIZE);

	// 映射UART寄存器，用于串口输入输出
	kernelPageMap(kPageTable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

	// 映射MMIO的硬盘寄存器，可读可写
	// TODO: 内核的page映射与用户态的不同。用户态映射需要将ref加1，而内核不用。
	// 考虑分别设两个函数
	kernelPageMap(kPageTable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

	// PLIC寄存器
	kernelPageMap(kPageTable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

	// 内核代码段
	kernelPageMap(kPageTable, KERNBASE, KERNBASE, (uint64)end_text - KERNBASE, PTE_R | PTE_X);

	PteEdit pte = pageLookup(kPageTable, KERNBASE);
	assertMsg(pteToPa(pte) == KERNBASE, "map error! va(0x%016lx) mapped to pa(0x%016lx)!\n",
		  KERNBASE, pteToPa(pte));

	// 内核数据段(就是代码段的结束位置到物理内存的最高位置)
	uint64 physicalTop = MEMBASE + memInfo.size;
	kernelPageMap(kPageTable, (uint64)end_text, (uint64)end_text,
		      physicalTop - (uint64)end_text, PTE_R | PTE_W);

	// Trampoline段的映射
	kernelPageMap(kPageTable, TRAMPOLINE, PGROUNDDOWN((u64)trampoline), PGSIZE, PTE_R | PTE_X);
	assert(pteToPa(pageLookup(kPageTable, TRAMPOLINE)) == (u64)trampoline);

	log("Init kernel page table SUCCESS!\n");
	return kPageTable;
}

static void memoryTest() {
	PteEdit pte;
	for (uint64 va = KERNBASE; va < MEMBASE + memInfo.size; va += PGSIZE) {
		pte = pageLookup(kernelPageTable, va);
		assertMsg(pteToPa(pte) == va, "map error! va(0x%016lx) mapped to pa(0x%016lx)!\n",
			  va, pteToPa(pte));
	}
	log("memory map test passed!\n");
}

/**
 * @brief 初始化内存页空闲链表
 */
static void pageInit() {
	log("Begin Initing page free list...\n");

	LIST_INIT(&pageFreeList);

	// 将前半部分已经用掉的内存的使用次数标记为1
	extern void *freeMem;
	for (uint64 i = MEMBASE; i < (uint64)freeMem; i += PGSIZE) {
		uint64 index = (i - MEMBASE) / PGSIZE;
		pages[index].ref = 1;
	}

	// 倒序插入空闲页，使队列中的内存从头至尾地址递增
	uint64 beginPage = ((uint64)freeMem - MEMBASE) / PGSIZE;
	for (uint64 i = npages - 1; i >= beginPage; i--) {
		pages[i].ref = 0;
		LIST_INSERT_HEAD(&pageFreeList, &pages[i], link);
	}

	log("Init page free list SUCCESS!\n");
}

/**
 * @brief 初始化物理内存管理
 */
void initKernelMemory() {
	kinit();	     // 初始化内核alloc
	initDataStructure(); // 使用kalloc分配一些内核可能会用到的数据结构
	pageInit();	     // 初始化空闲链表，以待pageAlloc
	kernelPageTable = initKvmPageTable();
	memoryTest();
}

/**
 * 初始化内核分页前需要使用的数据结构
 */
static void initDataStructure() {
	npages = memInfo.size / PGSIZE;
	pages = kalloc(npages * sizeof(Page));
}
