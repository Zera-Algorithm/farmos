#include <lib/string.h>
#include <mm/memlayout.h>
#include <mm/memory.h>

extern void flushTlb();
Page *pages;

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
	Pte *pte = pageDirWalk(pageDir, va, False); // zrp: 这里应该是False吧？
	return pte == NULL ? 0 : *pte;
}

/**
 * @brief 为内核映射一页的虚拟地址，该虚拟地址一般与物理地址线性对应
 * @note 我们不操作Page结构体，不增加ref次数
 */
MemErrCode kernelPageInsert(Pte *pageDir, uint64 va, uint64 pa, uint64 perm) {
	// 我们规定内核映射时，页表项应当始终不存在
	Pte *pte = pageDirWalk(pageDir, va, False);
	assertMsg(pte == NULL || !(*pte & PTE_V), "pte = 0x%08x, PTE_V = %d\n", pte,
		  (pte == NULL) ? -1 : (*pte & PTE_V));

	// 页表项已经不存在，创建新的页表项
	pte = pageDirWalk(pageDir, va, True);

	// 建立新的映射，但不需要管理Page结构体的ref映射次数
	*(PteEdit *)pte = (paToPte(pa) | perm | PTE_V);
	// log("va = 0x%016x, pa = 0x%016x, pte = 0x%016x\n", va, pa, pte);

	return SUCCESS;
}

/**
 * @brief 为内核映射连续的一段内存地址
 * @note 调用者应当保证va, pa, size页对齐
 * @author zrp
 */
MemErrCode kernelPageMap(Pte *pageDir, uint64 va, uint64 pa, uint64 size, uint64 perm) {
	for (uint64 i = 0; i < size; i += PAGE_SIZE) {
		catchMemErr(kernelPageInsert(pageDir, va + i, pa + i, perm));
	}
	return SUCCESS;
}

MemErrCode pageInsert(Pte *pageDir, uint64 va, uint64 pa, uint64 perm) {
	// 如果页表项已经存在，抹除其内容（可优化）
	if (pageDirWalk(pageDir, va, False) != NULL) { // zrp: 这里应该是False？
		catchMemErr(pageRemove(pageDir, va));
	}
	// 页表项已经不存在，创建新的页表项
	Pte *pte = pageDirWalk(pageDir, va, True);

	log("begin modify Pte...\n");

	// 建立新的映射
	modifyPte(pte, paToPte(pa) | perm | PTE_V);

	log("end insert of va 0x%016x, pa 0x%016x\n", va, pa);
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
	log("get page %d\n", pp - pages);
	return pp;
}

static Pte *pageDirWalk(Pte *pageDir, uint64 va, uint8 create) {
	Pte *curPageTable = pageDir;

	// 一级页index不能大于2！
	assertMsg(PTX(va, 1) <= 2, "first pgtable offset: %d, va = 0x%016lx\n", PTX(va, 1), va);

	// 从顶级页目录开始，依次获取每一级页表
	for (int i = 1; i < 3; i++) {
		// 获取当前页表项
		Pte *curPte = &curPageTable[PTX(va, i)];
		// 检查当前页表项是否存在
		if (*curPte & PTE_V) {
			// 如果存在，获取下一级页表
			// zrp: 这里是一个bug!
			curPageTable = (Pte *)pteToPa(*curPte);
		} else {
			// 如果不存在，创建下一级页表
			if (create) {
				log("create a page for level %d in va 0x%016lx\n", i, va);
				Page *newPage = pageAlloc();
				newPage->ref += 1;
				// 将新页表的物理地址写入当前页表项
				modifyPte(curPte, pageToPte(newPage) | PTE_V);
				// 将新页表的虚拟地址赋值给 curPageTable
				curPageTable = (Pte *)pageToPa(newPage); // zrp: bug!
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
	if ((*pte & PTE_V) && pteToPa(*pte) >= MEMBASE) {
		Page *page = pteToPage(*pte);
		page->ref -= 1;
		if (page->ref == 0) {
			// 如果引用计数为 0，将该页加入空闲页链表
			LIST_INSERT_HEAD(&pageFreeList, page, link);
		}
	}
	// 建立新的映射
	*(PteEdit *)pte = value;
	// 要求*pte表项指向的是MEMBASE以上的内存，不是设备内存，否则没有必要设置其所属的page
	if ((*pte & PTE_V) && pteToPa(*pte) >= MEMBASE) {
		pteToPage(*pte)->ref += 1;
	}

	log("finish modidy Pte!\n");
	// 我们修改了页表，需要刷新 TLB(zrp: 不一定，可能操作的页表与tlb无关)
	// TODO: 需要判断是否需要刷新TLB
	// flushTlb();
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
