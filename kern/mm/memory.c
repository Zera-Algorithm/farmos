#include <lib/string.h>
#include <mm/memlayout.h>
#include <mm/memory.h>
#include <riscv.h>

// todo:加锁

// 空闲物理页链表
extern PageList pageFreeList;

// 模块内私有函数声明

/**
 * @brief 分配一个物理页，仅在内存管理模块内使用，调用者必须维护该物理页的引用计数
 * @return 成功返回分配到的物理页物理地址，失败会使系统自陷
 */
static Page *_pageAlloc();

/**
 * @brief 从虚拟地址 va 所在的页表中获取对应的页表项
 * @param pageDir 顶级页目录
 * @param va 虚拟地址
 * @param create 为真时会创建不存在的页表
 * @return 成功返回对应的页表项地址，无法获取到有效的页表项且 create 为假时返回 NULL
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

#define curPgdir ((Pte *)CUR_PGDIR)

// 接口函数实现

Paddr pageAlloc() {
	Page *pp = _pageAlloc();
	return pageToPa(pp);
}

Pte pageLookup(Pte *pageDir, uint64 va) {
	Pte *pte = pageDirWalk(pageDir, va, false); // zrp: 这里应该是False吧？
	return pte == NULL ? 0 : *pte;
}

MemErrCode pageInsert(Pte *pageDir, uint64 va, uint64 pa, uint64 perm) {
	// 如果页表项已经存在，抹除其内容（可优化）
	Pte *originPte = pageDirWalk(pageDir, va, false);
	if (originPte != NULL && (*originPte & PTE_V)) {
		catchMemErr(pageRemove(pageDir, va));
	}
	// 页表项已经不存在，创建新的页表项
	Pte *pte = pageDirWalk(pageDir, va, true);

	log("begin modify Pte...\n");

	// 建立新的映射
	modifyPte(pte, paToPte(pa) | perm | PTE_V);
	if (curPgdir == pageDir) {
		flushTlb(va);
	}

	log("end insert of va 0x%016lx, pa 0x%016lx\n", va, pa);
	return SUCCESS;
}

MemErrCode pageRemove(Pte *pageDir, uint64 va) {
	Pte *pte = pageDirWalk(pageDir, va, false);
	if (!(*pte & PTE_V)) {
		return NO_VALID_MAP;
	}
	// 维护引用计数并清除页表项内容
	clearPte(pte);
	if (curPgdir == pageDir) {
		flushTlb(va);
	}
	return SUCCESS;
}

// 内存管理模块内部使用的函数

static Page *_pageAlloc() {
	// 尝试从空闲页链表中取出一个空闲页
	Page *pp = LIST_FIRST(&pageFreeList);
	if (pp == NULL) {
		panic("no more pages to alloc!\n");
	}
	// 从空闲页链表中移除该页
	LIST_REMOVE(pp, link);
	// 清空页面内容并返回
	memset((void *)pageToPa(pp), 0, PAGE_SIZE);
	// log("get page %d\n", pageToPpn(pp));
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
			// zrp: 这里是一个bug!
			curPageTable = (Pte *)pteToPa(*curPte);
		} else {
			// 如果不存在，创建下一级页表
			if (create) {
				// log("create a page for level %d in va 0x%016lx\n", i, va);
				Page *newPage = _pageAlloc();
				newPage->ref += 1;
				// 将新页表的物理地址写入当前页表项
				modifyPte(curPte, pageToPte(newPage) | PTE_V);
				if (curPgdir == pageDir) {
					flushTlb(va);
				}
				// 将新页表的虚拟地址赋值给 curPageTable
				curPageTable = (Pte *)pageToPa(newPage);
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

	// log("finish modidy Pte!\n");
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
}

// 内核初始化使用的函数

/**
 * @brief 为内核映射一页的虚拟地址，该虚拟地址一般与物理地址线性对应
 * @note 我们不操作Page结构体，不增加ref次数
 */
void kernelPageInsert(Pte *pageDir, uint64 va, uint64 pa, uint64 perm) {
	// 我们规定内核映射时，页表项应当始终不存在
	Pte *pte = pageDirWalk(pageDir, va, false);
	assertMsg(pte == NULL || !(*pte & PTE_V), "pte = 0x%08x, PTE_V = %d\n", pte,
		  (pte == NULL) ? -1 : (*pte & PTE_V));

	// 页表项已经不存在，创建新的页表项
	pte = pageDirWalk(pageDir, va, true);

	// 建立新的映射，但不需要管理Page结构体的ref映射次数
	*(PteEdit *)pte = (paToPte(pa) | perm | PTE_V);
	// log("va = 0x%016x, pa = 0x%016x, pte = 0x%016x\n", va, pa, pte);
}
