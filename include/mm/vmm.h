#ifndef _VMM_H
#define _VMM_H

#include <mm/pmm.h>
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
#define PTE_PPNSHIFT (10ull)		      // 页表项中物理页号的偏移量
#define PTE_PPNMASK ((~0ull) << PTE_PPNSHIFT) // 页表项中物理页号的掩码
#define PAGE_LEVELS (3ull)		      // 页表级数
#define PAGE_INDEX_LEN (9ull)		      // 页表项索引长度
#define PTX(va, level)                                                                             \
	(((u64)(va) >> (PAGE_SHIFT + (PAGE_LEVELS - (level)) * PAGE_INDEX_LEN)) &                  \
	 0x1ff) // 获取虚拟地址 va 在第 level 级页表中的索引

// 页表相关声明
typedef const u64 Pte;

void vmmInit();

u64 vmAlloc();
Pte ptLookup(Pte *pgdir, u64 va);
err_t ptMap(Pte *pgdir, u64 va, u64 pa, u64 perm);
err_t ptUnmap(Pte *pgdir, u64 va);

Pte paToPte(u64 pa);
u64 pteToPa(Pte pte);

// deprecated
#define pageAlloc() vmAlloc()
#define pageLookup(pgdir, va) ptLookup(pgdir, va)
#define pageInsert(pgdir, va, pa, perm) ptMap(pgdir, va, pa, perm)
#define pageRemove(pgdir, va) ptUnmap(pgdir, va)

#endif // _VMM_H