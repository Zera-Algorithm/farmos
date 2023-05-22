

#include <lib/error.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

extern Pte *curPgdir();
extern void flushTlb();

Pte *kernPd;

// 纯接口函数

inline Pte paToPte(u64 pa) {
	return (pa >> PAGE_SHIFT) << PTE_PPNSHIFT;
}

inline u64 pteToPa(Pte pte) {
	return (pte >> PTE_PPNSHIFT) << PAGE_SHIFT;
}

inline Pte pageToPte(Page *p) {
	return paToPte(MEMBASE) + (pageToPpn(p) << PTE_PPNSHIFT);
}

inline Page *pteToPage(Pte pte) {
	assert(pteToPa(pte) > MEMBASE);
	return paToPage(pteToPa(pte));
}

// 内部功能接口函数 TODO::STATIC

void ptModify(Pte *pte, Pte value) {
	// 取消原先的映射
	if ((*pte & PTE_V) && pteToPa(*pte) >= MEMBASE) {
		Page *page = pteToPage(*pte);
		pmPageDecRef(page);
	}
	// 建立新的映射
	*(u64 *)pte = value;
	// 要求*pte表项指向的是MEMBASE以上的内存，不是设备内存，否则没有必要设置其所属的page
	if ((*pte & PTE_V) && pteToPa(*pte) >= MEMBASE) {
		pmPageIncRef(pteToPage(*pte));
	}

	// log("finish modidy Pte!\n");
}

void ptClear(Pte *pte) {
	// 取消原先的映射
	if (*pte & PTE_V) {
		Page *page = pteToPage(*pte);
		pmPageDecRef(page);
	}
	*(u64 *)pte = 0;
}

Pte *ptWalk(Pte *pageDir, u64 va, bool create) { // TODO:STATIC!!!!!!!!!!!!!!!!!
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
				log("\tcreate a page for level %d in va 0x%016lx\n", i, va);
				Page *newPage = pmAlloc();
				pmPageIncRef(newPage);
				// 将新页表的物理地址写入当前页表项
				ptModify(curPte, pageToPte(newPage) | PTE_V);
				if (curPgdir() == pageDir) {
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

// 初始化函数
static void vmInitMap(u64 pa, u64 va, u64 len, u64 perm) {
	for (u64 off = 0; off < len; off += PAGE_SIZE) {
		*(u64 *)ptWalk(kernPd, va + off, true) = (paToPte(pa + off) | perm | PTE_V);
	}
}

static void memoryTest() {
	u64 pte;
	for (uint64 va = KERNBASE; va < pmTop(); va += PAGE_SIZE) {
		pte = ptLookup(kernPd, va);
		assertMsg(pteToPa(pte) == va, "map error! va(0x%016lx) mapped to pa(0x%016lx)!\n",
			  va, pteToPa(pte));
	}
	log("Passed Kernel MemMap Test!\n");
}

void vmmInit() {
	// 第一步：初始化内核页目录
	log("Virtual Memory Init Start\n");
	kernPd = (Pte *)pageToPa(pmAlloc());

	// 第二步：映射UART寄存器，用于串口输入输出
	vmInitMap(UART0, UART0, PAGE_SIZE, PTE_R | PTE_W);

	// 第三步：映射MMIO的硬盘寄存器，可读可写
	vmInitMap(VIRTIO0, VIRTIO0, PAGE_SIZE, PTE_R | PTE_W);

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
	log("Virtual Memory Init Finished, `vm` Functions Available!\n");
}

// 功能接口函数

u64 vmAlloc() {
	Page *pp = pmAlloc();
	return pageToPa(pp);
}

Pte ptLookup(Pte *pgdir, u64 va) {
	Pte *pte = ptWalk(pgdir, va, false);
	return pte == NULL ? 0 : *pte;
}

err_t ptMap(Pte *pgdir, u64 va, u64 pa, u64 perm) {
	// 如果页表项已经存在，抹除其内容（可优化）
	Pte *originPte = ptWalk(pgdir, va, false);
	if (originPte != NULL && (*originPte & PTE_V)) {
		panic_on(ptUnmap(pgdir, va));
	}
	// 页表项已经不存在，创建新的页表项
	Pte *pte = ptWalk(pgdir, va, true);

	log("begin modify Pte...\n");

	// 建立新的映射
	ptModify(pte, paToPte(pa) | perm | PTE_V);
	if (curPgdir() == pgdir) {
		flushTlb();
	}

	log("end insert of va 0x%016lx, pa 0x%016lx\n", va, pa);
	return 0;
}

err_t ptUnmap(Pte *pgdir, u64 va) {
	Pte *pte = ptWalk(pgdir, va, false);
	if (!(*pte & PTE_V)) {
		return -E_NO_MAP;
	}
	// 维护引用计数并清除页表项内容
	ptClear(pte);
	if (curPgdir() == pgdir) {
		flushTlb(va);
	}
	return 0;
}