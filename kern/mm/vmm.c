#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lock/mutex.h>
#include <mm/mmu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <dev/sd.h>

Pte *kernPd;

// 纯接口函数

static inline Pte pageToPte(Page *p) {
	return paToPte(MEMBASE) + (pageToPpn(p) << PTE_PPNSHIFT);
}

static inline Page *pteToPage(Pte pte) {
	assert(pteToPa(pte) > MEMBASE);
	return paToPage(pteToPa(pte));
}

// 映射操作包裹（解引用映射时使用，若传入页表项指向实际内存页面则更新映射次数）
static inline void pp_dec_on_valid(pte_t pte) {
	if (pte & PTE_V && pteToPa(pte) >= MEMBASE) {
		pmPageDecRef(pteToPage(pte));
	}
}

static inline void pp_inc_on_valid(pte_t pte) {
	if (pte & PTE_V && pteToPa(pte) >= MEMBASE) {
		pmPageIncRef(pteToPage(pte));
	}
}

static inline void flush_tlb_if_need(pte_t *pd, u64 va) {
	if (ptFetch() == pd) {
		tlbFlush(va);
	}
}

// 内部功能接口函数

/**
 * @brief 修改传入的页表项指针指向的页表项至传入的新值，若指向物理页相同则仅修改权限
 * @note 在 FarmOS 中，PTE_V 代表该页表项的物理页号有效，即存在对应物理页。因此对于修改前后会检查 PTE_V 标志并对物理页的映射次数进行维护。
 */
static void ptModify(Pte *pte, Pte value) {
	u64 oldpa = pteToPa(*pte);
	u64 newpa = pteToPa(value);
	if (oldpa == newpa) {
		// 页表项指向的物理页没有变化，仅修改权限
		*pte = value;
	} else {
		// 页表项指向的物理页发生了变化，需要修改映射
		pp_dec_on_valid(*pte);
		*pte = value;
		pp_inc_on_valid(value);
	}
}

/**
 * @brief 将传入的页表项指针指向的页表项清零，若指向物理页则取消映射 
 */
static void ptClear(Pte *pte) {
	// 取消原先的映射
	pp_dec_on_valid(*pte);
	*pte = 0;
}

static Pte *ptWalk(Pte *pageDir, u64 va, bool create) {
	Pte *curPageTable = pageDir;

	// 从顶级页目录开始，依次获取每一级页表
	for (int i = 1; i < 3; i++) {
		// 获取当前页表项
		Pte *curPte = &curPageTable[PTX(va, i)];
		// 检查当前页表项是否存在
		if (*curPte & PTE_V) {
			// 如果存在，获取下一级页表
			curPageTable = (Pte *)pteToPa(*curPte);
		} else {
			// 如果不存在，创建下一级页表
			if (create) {
				log(LEVEL_MODULE, "\tcreate a page for level %d in va 0x%016lx\n",
				    i, va);
				Page *newPage = pmAlloc();
				// 将新页表的物理地址写入当前页表项
				ptModify(curPte, pageToPte(newPage) | PTE_V);
				flush_tlb_if_need(pageDir, va);
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

// 初始化函数
static void vmInitMap(u64 pa, u64 va, u64 len, u64 perm) {
	for (u64 off = 0; off < len; off += PAGE_SIZE) {
		*(u64 *)ptWalk(kernPd, va + off, true) = (paToPte(pa + off) | perm | PTE_V | PTE_MACHINE);
	}
}

static void memoryTest() {
	u64 pte;
	for (uint64 va = KERNBASE; va < pmTop(); va += PAGE_SIZE) {
		pte = ptLookup(kernPd, va);
		assert(pteToPa(pte) == va);
	}
	log(LEVEL_GLOBAL, "Passed Kernel MemMap Test!\n");
}

mutex_t kvmlock;

void vmmInit() {
	// 第零步：初始化模块锁
	mtx_init(&kvmlock, "kvmlock", false, MTX_SPIN | MTX_RECURSE);

	// 第一步：初始化内核页目录
	log(LEVEL_GLOBAL, "Virtual Memory Init Start\n");
	kernPd = (Pte *)pageToPa(pmAlloc());

	// 第二步：映射UART寄存器，用于串口输入输出
	vmInitMap(UART0, UART0, PAGE_SIZE, PTE_R | PTE_W);

	// SD卡
	vmInitMap(SPI_CTRL_ADDR, SPI_CTRL_ADDR, PAGE_SIZE, PTE_R | PTE_W);

	// 第三步：映射MMIO的硬盘寄存器，可读可写
	vmInitMap(VIRTIO0, VIRTIO0, PAGE_SIZE, PTE_R | PTE_W);

	vmInitMap(RTC_BASE, RTC_BASE, PAGE_SIZE, PTE_R | PTE_W);

	// 第四步：PLIC寄存器
	vmInitMap(PLIC, PLIC, 0x400000, PTE_R | PTE_W); // todo:literal

	// 第五步：内核代码段
	extern char end_text[];
	vmInitMap(KERNBASE, KERNBASE, (u64)end_text - KERNBASE, PTE_R | PTE_X);

	// 第六步：内核数据段(就是代码段的结束位置到物理内存的最高位置)
	vmInitMap((u64)end_text, (u64)end_text, pmTop() - (u64)end_text, PTE_R | PTE_W);

	// 第七步：Trampoline段的映射
	extern char trampoline[];
	vmInitMap(PGROUNDDOWN((u64)trampoline), TRAMPOLINE, PAGE_SIZE, PTE_R | PTE_X);

	// 第八步：测试
	memoryTest();
	log(LEVEL_GLOBAL, "Virtual Memory Init Finished, `vm` Functions Available!\n");
}

// 功能接口函数

/**
 * @brief 在内核中申请一个物理页，返回其物理地址
 */
u64 kvmAlloc() {
	mtx_lock(&kvmlock);
	Page *pp = pmAlloc();
	pmPageIncRef(pp);
	mtx_unlock(&kvmlock);
	return pageToPa(pp);
}

void kvmFree(u64 pa) {
	mtx_lock(&kvmlock);
	Page *pp = paToPage(pa);
	pmPageDecRef(pp);
	mtx_unlock(&kvmlock);
}

u64 vmAlloc() {
	mtx_lock(&kvmlock);
	Page *pp = pmAlloc();
	mtx_unlock(&kvmlock);
	return pageToPa(pp);
}

Pte ptLookup(Pte *pgdir, u64 va) {
	mtx_lock(&kvmlock);
	Pte *pte = ptWalk(pgdir, va, false);
	mtx_unlock(&kvmlock);
	return pte == NULL ? 0 : *pte;
}

/**
 * @brief 修改已有映射、或添加映射 
 */
err_t ptMap(Pte *pgdir, u64 va, u64 pa, u64 perm) {
	mtx_lock(&kvmlock);
	// 遍历页表尝试获得 va 对应的页表项地址
	Pte *pte = ptWalk(pgdir, va, false);
	// 若不存在对应的页表项，则进行创建
	if (pte == NULL) {
		pte = ptWalk(pgdir, va, true);
	}

	/**
	 * 对于页表项的 3 种状态间转换（有效、被动有效、无效）：
	 * 1. 有效 -> 有效：
	 * 		修改页表项内容（分支 1）
	 * 2. 有效 -> 被动有效：
	 * 		不应该存在这种情况
	 * 3. 有效 -> 无效：
	 * 		不应该存在这种情况，应该使用 ptUnmap
	 * 4. 被动有效 -> 有效：
	 * 		分配页面，修改页表项内容（分支 3）
	 * 5. 被动有效 -> 被动有效：
	 * 		修改页表项内容，更新权限（分支 2）
	 * 6. 被动有效 -> 无效：
	 * 		不应该存在这种情况，应该使用 ptUnmap
	 * 7. 无效 -> 有效：
	 * 		分配页面，修改页表项内容（分支 3）
	 * 8. 无效 -> 被动有效：
	 * 		修改页表项内容，更新权限（分支 2）
	 * 9. 无效 -> 无效：
	 * 		这是在干什么？
	 */
	if (*pte & PTE_V) {
		// 原页表项有效时，修改映射（此时不应该是添加被动映射）
		assert(!(*pte & PTE_PASSIVE));
		ptModify(pte, paToPte(pa) | perm | PTE_V | PTE_MACHINE);
		
	} else if (perm & PTE_PASSIVE) {
		// 原页表项无效，添加被动映射（传入的物理地址必须为零）
		assert(pa == 0);
		ptModify(pte, perm);
		mtx_unlock(&kvmlock);
		return 0; // 直接返回，不用刷新 TLB
	} else {
		// 原页表项无效，添加有效映射，外部已申请了页面
		assert(pa >= MEMBASE);
		ptModify(pte, paToPte(pa) | perm | PTE_V | PTE_MACHINE);
	}

	// 如果操作的是当前页表，刷新 TLB
	flush_tlb_if_need(pgdir, va);
	mtx_unlock(&kvmlock);
	return 0;
}

err_t ptUnmap(Pte *pgdir, u64 va) {
	mtx_lock(&kvmlock);
	Pte *pte = ptWalk(pgdir, va, false);
	if (!(*pte & PTE_V) && !(*pte & PTE_PASSIVE)) {
		return -E_NO_MAP;
	}
	// 维护引用计数并清除页表项内容
	ptClear(pte);

	flush_tlb_if_need(pgdir, va);

	mtx_unlock(&kvmlock);
	return 0;
}
