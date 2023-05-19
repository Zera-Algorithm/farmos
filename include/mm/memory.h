#ifndef _MEMORY_H
#define _MEMORY_H

#include <lib/printf.h>
#include <lib/queue.h>
#include <types.h>

// 内核映射：0x80000000+ 的内存地址在内核页表中均为直接映射
#define NULL ((void *)0)
#define True (1)
#define False (0)

// 页表项硬件标志
#define PTE_V (1 << 0) // 有效位（Valid）
#define PTE_R (1 << 1) // 可读位（Readable）
#define PTE_W (1 << 2) // 可写位（Writable）
#define PTE_X (1 << 3) // 可执行位（eXecutable）
#define PTE_U (1 << 4) // 用户位（User）
#define PTE_G (1 << 5) // 全局位（Global）
#define PTE_A (1 << 6) // 访问位（Accessed）
#define PTE_D (1 << 7) // 脏位（Dirty）

// 基于 Sv39 的页表结构
#define PAGE_SHIFT (12ull)	    // 基页大小为 4KB
#define PAGE_SIZE (1 << PAGE_SHIFT) // 基页大小为 4KB

#define PTE_PPNSHIFT (10ull)		      // 页表项中物理页号的偏移量
#define PTE_PPNMASK ((~0ull) << PTE_PPNSHIFT) // 页表项中物理页号的掩码

#define PAGE_LEVELS (3ull)    // 页表级数
#define PAGE_INDEX_LEN (9ull) // 页表项索引长度

#define PTX(va, level)                                                                             \
	(((uint64)(va) >> (PAGE_SHIFT + (PAGE_LEVELS - (level)) * PAGE_INDEX_LEN)) &               \
	 0x1ff) // 获取虚拟地址 va 在第 level 级页表中的索引
#define PTE_ADDR(pte) ((uint64)(pte) & ~0xfff) // 获取页表项 pte 所指向的物理页地址

// 物理页面结构体相关声明
struct Page;
typedef LIST_HEAD(PageList, Page) PageList;
typedef LIST_ENTRY(Page) PageLink;
typedef struct Page {
	uint32 ref; // 编辑途径仅有 modifyPte/clearPte/alloc后的初始化
	PageLink link;
} Page;

// 页表相关声明
typedef const uint64 Pte;
typedef uint64 PteEdit;

// 内存管理模块功能接口
typedef uint32 MemErrCode;

/**
 * @brief 在 pageDir 所指向的页表中，查找虚拟地址 va 对应的页表项
 * @param pageDir 顶级页目录
 * @param va 虚拟地址
 * @return 成功返回对应的页表项，失败返回 0
 */
Pte pageLookup(Pte *pageDir, uint64 va);
MemErrCode pageInsert(Pte *pageDir, uint64 va, uint64 pa, uint64 perm);
MemErrCode pageRemove(Pte *pageDir, uint64 va);
MemErrCode kernelPageMap(Pte *pageDir, uint64 va, uint64 pa, uint64 size, uint64 perm);

// 内存管理错误码

enum MemErrorCode { SUCCESS = 0, NO_VALID_MAP };

#define catchMemErr(expr)                                                                          \
	do {                                                                                       \
		MemErrCode err = (expr);                                                           \
		if (err != SUCCESS) {                                                              \
			return err;                                                                \
		}                                                                                  \
	} while (0)

// 页结构体相关操作
inline uint64 pageToPpn(Page *p) {
	extern Page *pages;
	return p - pages;
}

inline uint64 pageToPa(Page *p) {
	return 0x80000000ul + (pageToPpn(p) << PAGE_SHIFT); // TODO: literal
}

inline uint64 paToPte(uint64 pa) {
	return (pa >> PAGE_SHIFT) << PTE_PPNSHIFT; // TODO: literal
}

inline uint64 pageToPte(Page *p) {
	return paToPte(0x80000000ul) + (pageToPpn(p) << PTE_PPNSHIFT); // TODO: literal
}

inline uint64 pteToPa(uint64 pte) {
	return (pte >> PTE_PPNSHIFT) << PAGE_SHIFT; // TODO: literal
}

inline Page *paToPage(uint64 pa) {
	extern Page *pages;
	return &pages[(pa - 0x80000000ul) >> PAGE_SHIFT]; // TODO: literal
}

inline Page *pteToPage(uint64 pte) {
	// zrp: 不需要这个assert，因为映射设备内存的时候可以映射物理地址小于0x80000000的地址
	// assert(pteToPa(pte) > 0x80000000ul); // TODO: literal
	extern Page *pages;
	return paToPage(pteToPa(pte));
}

void enablePagingHart();
void initKernelMemory();
void flushTlb();

#endif // _MEMORY_H
