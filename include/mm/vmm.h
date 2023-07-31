#ifndef _VMM_H
#define _VMM_H

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

#ifdef QEMU
#define PTE_MACHINE 0
#else // Sifive
#define PTE_MACHINE (PTE_A | PTE_D)
#endif

// 用户定义标识
#define PTE_COW (1 << 8) // 写时复制位
#define PTE_SHARED (1 << 9) // 共享位

#define PTE_PASSIVE(pte) ((!(pte & PTE_V)) && (pte & PTE_U))

// 基于 Sv39 的页表结构
#define PTE_PPNSHIFT (10ull)		      // 页表项中物理页号的偏移量
#define PTE_PPNMASK ((~0ull) << PTE_PPNSHIFT) // 页表项中物理页号的掩码
#define PTE_PERMMASK (~PTE_PPNMASK)	      // 页表项中权限位的掩码
#define PAGE_LEVELS (3ull)		      // 页表级数
#define PAGE_INDEX_LEN (9ull)		      // 页表项索引长度
#define PAGE_INDEX_MAX (1ull << PAGE_INDEX_LEN)
#define PTX(va, level)                                                                             \
	(((u64)(va) >> (PAGE_SHIFT + (PAGE_LEVELS - (level)) * PAGE_INDEX_LEN)) &                  \
	 0x1ff) // 获取虚拟地址 va 在第 level 级页表中的索引

// 获取PTE中的PERM部分
#define PTE_PERM(pte) ((pte) & ((1 << PTE_PPNSHIFT) - 1))

void vmmInit();

// 进行了引用计数，必须保证在使用完毕后使用 kvmFree 进行释放
u64 kvmAlloc() __attribute__((warn_unused_result));
// 传入的物理页必须通过是 kvmAlloc 分配的
void kvmFree(u64 pa);

u64 vmAlloc() __attribute__((warn_unused_result)); // 未进行引用计数，必须保证使用 ptMap 进行映射
err_t ptMap(Pte *pgdir, u64 va, u64 pa, u64 perm) __attribute__((warn_unused_result));
err_t ptUnmap(Pte *pgdir, u64 va) __attribute__((warn_unused_result));

Pte ptLookup(Pte *pgdir, u64 va) __attribute__((warn_unused_result));

static inline Pte paToPte(u64 pa) {
	return (pa >> PAGE_SHIFT) << PTE_PPNSHIFT;
}

static inline u64 pteToPa(Pte pte) {
	return (pte >> PTE_PPNSHIFT) << PAGE_SHIFT;
}

#endif // _VMM_H
