#include <lib/string.h>
#include <mm/memory.h>

extern void flushTlb();

// 空闲物理页链表
extern PageList pageFreeList;

/**
 * @brief 分配一个物理页，仅在内存管理模块内使用，调用者必须维护该物理页的引用计数
 * @return 成功返回分配到的物理页，失败会使系统自陷
 */
static Page *pageAlloc();

/**
 * @brief 从虚拟地址 va 所在的页表中获取对应的页表项
 * @param pageDir 顶级页目录
 * @param va 虚拟地址
 * @param create 为真时会创建不存在的页表
 * @return 成功返回 SUCCESS，无法获取到有效的页表项且 create 为假时返回 NO_VALID_MAP
 */
static Pte *pageDirWalk(Pte *pageDir, uint64 va, uint8 create);

/**
 * @brief 修改页表项内容的函数，仅在内存管理模块内使用，该函数会自动维护引用计数
 * @param pte 页表项地址
 * @param pa 物理页地址
 * @param perm 页表项权限
 * @note 该函数内直接调用了 flushTlb 函数，可以优化
 */
static void modifyPte(Pte *pte, Pte value);

/**
 * @brief 清除页表项内容，仅在内存管理模块内使用，该函数会自动维护引用计数
 * @param pte 页表项地址
 * @note 该函数内直接调用了 flushTlb 函数，可以优化
 */
static void clearPte(Pte *pte);

Pte pageLookup(Pte *pageDir, uint64 va) {
	Pte *pte = pageDirWalk(pageDir, va, True);
	return pte == NULL ? 0 : *pte;
}

MemErrCode pageInsert(Pte *pageDir, uint64 va, uint64 pa, uint64 perm) {
	// 如果页表项已经存在，抹除其内容（可优化）
	if (pageDirWalk(pageDir, va, True) != NULL) {
		catchMemErr(pageRemove(pageDir, va));
	}
	// 页表项已经不存在，创建新的页表项
	Pte *pte = pageDirWalk(pageDir, va, True);

	// 建立新的映射
	modifyPte(pte, paToPte(pa) | perm | PTE_V);
	return SUCCESS;
}

MemErrCode pageRemove(Pte *pageDir, uint64 va) {
	Pte *pte = pageDirWalk(pageDir, va, False);
	if (!(*pte & PTE_V)) {
		return NO_VALID_MAP;
	}
	// 维护引用计数并清除页表项内容
	clearPte(pte);
	return SUCCESS;
}

// 内存管理模块内部使用的函数

static Page *pageAlloc() {
	// 尝试从空闲页链表中取出一个空闲页
	Page *pp = LIST_FIRST(&pageFreeList);
	if (pp == NULL) {
		while (1)
			;
	}
	// 从空闲页链表中移除该页
	LIST_REMOVE(pp, link);
	// 清空页面内容并返回
	memset((void *)pageToPa(pp), 0, PAGE_SIZE);
	return pp;
}

static Pte *pageDirWalk(Pte *pageDir, uint64 va, uint8 create) {
	Pte *curPageTable = pageDir;

	// 从顶级页目录开始，依次获取每一级页表
	for (int i = 1; i < 3; i++) {
		// 获取当前页表项
		Pte *curPte = &curPageTable[PTX(va, i)];
		// 检查当前页表项是否存在
		if (*curPte & PTE_V) {
			// 如果存在，获取下一级页表
			curPageTable = (Pte *)PTE_ADDR(*curPte);
		} else {
			// 如果不存在，创建下一级页表
			if (create) {
				Page *newPage = pageAlloc();
				newPage->ref += 1;
				// 将新页表的物理地址写入当前页表项
				modifyPte(curPte, pageToPte(newPage) | PTE_V);
				// 将新页表的虚拟地址赋值给 curPageTable
				curPageTable = (Pte *)newPage;
			} else {
				return NULL;
			}
		}
	}
	// 返回最后一级页表项
	return &curPageTable[PTX(va, 3)];
}

static void modifyPte(Pte *pte, Pte value) {
	// 取消原先的映射
	if (*pte & PTE_V) {
		Page *page = pteToPage(*pte);
		page->ref -= 1;
		if (page->ref == 0) {
			// 如果引用计数为 0，将该页加入空闲页链表
			LIST_INSERT_HEAD(&pageFreeList, page, link);
		}
	}
	// 建立新的映射
	*(PteEdit *)pte = value;
	if (*pte & PTE_V) {
		pteToPage(*pte)->ref += 1;
	}
	// 我们修改了页表，需要刷新 TLB
	flushTlb();
}

static void clearPte(Pte *pte) {
	// 取消原先的映射
	if (*pte & PTE_V) {
		Page *page = pteToPage(*pte);
		page->ref -= 1;
		if (page->ref == 0) {
			// 如果引用计数为 0，将该页加入空闲页链表
			LIST_INSERT_HEAD(&pageFreeList, page, link);
		}
	}
	*(PteEdit *)pte = 0;
	// 我们修改了页表，需要刷新 TLB
	flushTlb();
}