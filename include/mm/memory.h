#ifndef _MEMORY_H
#define _MEMORY_H

#include <lib/printf.h>
#include <lib/queue.h>
#include <mm/memlayout.h>
#include <types.h>

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
#define PAGE_SHIFT (12ull)		      // 基页大小为 4KB
#define PAGE_SIZE (1 << PAGE_SHIFT)	      // 基页大小为 4KB
#define PTE_PPNSHIFT (10ull)		      // 页表项中物理页号的偏移量
#define PTE_PPNMASK ((~0ull) << PTE_PPNSHIFT) // 页表项中物理页号的掩码
#define PAGE_LEVELS (3ull)		      // 页表级数
#define PAGE_INDEX_LEN (9ull)		      // 页表项索引长度
#define PTX(va, level)                                                                             \
	(((uint64)(va) >> (PAGE_SHIFT + (PAGE_LEVELS - (level)) * PAGE_INDEX_LEN)) &               \
	 0x1ff) // 获取虚拟地址 va 在第 level 级页表中的索引

// 页表相关声明
typedef u64 Vaddr;
typedef u64 Paddr;
typedef const u64 Pte;
typedef u64 PteEdit;

// 物理页面结构体相关声明
struct Page;
typedef LIST_HEAD(PageList, Page) PageList;
typedef LIST_ENTRY(Page) PageLink;
typedef struct Page {
	u32 ref; // 编辑途径仅有 modifyPte/clearPte/alloc后的初始化
	PageLink link;
} Page;

// 内存管理模块功能接口
typedef uint32 MemErrCode;

/**
 * @brief 在 pageDir 所指向的页表中，查找虚拟地址 va 对应的页表项
 * @param pageDir 顶级页目录
 * @param va 虚拟地址
 * @return 成功返回对应的页表项，失败返回 0
 */
Pte pageLookup(Pte *pageDir, uint64 va);
Paddr pageAlloc();
MemErrCode pageInsert(Pte *pageDir, Vaddr va, Paddr pa, u64 perm);
MemErrCode pageRemove(Pte *pageDir, Vaddr va);

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

inline u64 pageToPpn(Page *p) {
	extern Page *pages;
	return p - pages;
}

inline Pte paToPte(Paddr pa) {
	return (pa >> PAGE_SHIFT) << PTE_PPNSHIFT;
}

inline Pte pageToPte(Page *p) {
	return paToPte(MEMBASE) + (pageToPpn(p) << PTE_PPNSHIFT);
}

inline Paddr pteToPa(Pte pte) {
	return (pte >> PTE_PPNSHIFT) << PAGE_SHIFT;
}

inline Paddr pageToPa(Page *p) {
	return MEMBASE + (pageToPpn(p) << PAGE_SHIFT);
}

inline Page *paToPage(Paddr pa) {
	extern Page *pages;
	return &pages[(pa - MEMBASE) >> PAGE_SHIFT];
}

inline Page *pteToPage(Pte pte) {
	assert(pteToPa(pte) > MEMBASE);
	return paToPage(pteToPa(pte));
}

void enablePagingHart();
void initKernelMemory();
void flushTlb();

#endif // _MEMORY_H
