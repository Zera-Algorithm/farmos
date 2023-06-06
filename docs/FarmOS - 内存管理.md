# FarmOS 内存管理

## 概述

FarmOS 的内存管理分为两部分，物理内存管理和虚拟内存管理。相关文件目录结果如下：
```
.
├── include
│   └── mm
│       ├── memlayout.h
│       ├── mmu.h
│       ├── pmm.h
│       └── vmm.h
├── kernel
│   └── mm
│       ├── mmu.c
│       ├── pmm.c
│       └── vmm.c

```

### 启动过程

在启动过程中，首先进行物理内存管理的初始化，然后进行虚拟内存管理的初始化。在虚拟内存管理初始化完成后，便可以使用虚拟内存管理的接口进行内存分配，同时也可以开启分页。

- `main()`：启动函数
	- `pmmInit()`：物理内存管理初始化
	- `vmmInit()`：虚拟内存管理初始化
	- `vmEnable()`：开启分页



## 物理内存管理

物理内存管理模块负责管理物理内存。物理内存模块使用页式物理内存管理算法，将物理内存划分为大小为 4KB 的页，每个页对应一个物理页帧，每个物理页帧对应一个物理页管理结构体 `struct Page`。物理页管理结构体 `struct Page` 的定义如下：

```c
struct Page {
	u32 ref;
	LIST_ENTRY(Page) link;
};
```

FarmOS 中采用空闲页面链表对物理页进行管理。
- 初始化时，将所有物理页加入空闲页面链表
- 分配物理页时，从空闲页面链表中取出一个物理页
- 释放物理页时，将物理页加入空闲页面链表

### 物理内存管理模块的初始化

物理内存管理模块的初始化函数为 `pmmInit()`，主要功能如下：
- 读取设备信息，获取内存信息，初始化内存页数组
- 为需要连续内存空间的模块分配内存
- 初始化空闲页面链表
在初始化后，便可以使用 `pmAlloc()`、`pmPageIncRef()`、`pmPageDecRef()` 函数使用物理内存。

```c
void pmmInit() {
	// 第一部分：读取设备信息，获取内存信息，初始化内存页数组
	u64 freemem = PGROUNDUP((u64)end); // 空闲内存页的起始地址
	npage = memInfo.size / PAGE_SIZE;  // 内存页数
	pages = pmInitPush(freemem, npage * sizeof(Page), &freemem); // 初始化内存页数组

	// 第二部分：为需要连续内存空间的模块分配内存
	extern void *example;
	example = pmInitPush(freemem, /* Size */, &freemem);
    // ...

	// 第三部分：初始化空闲页面链表
	LIST_INIT(&pageFreeList);
	u64 pageused = (freemem - MEMBASE) >> PAGE_SHIFT; // 已经使用的内存页数
	for (u64 i = 0; i < pageused; i++) {
		pages[i].ref = 1;
	}

	for (u64 i = pageused; i < npage; i++) {
		LIST_INSERT_HEAD(&pageFreeList, &pages[i], link);
	}
}
```

### 物理内存管理模块接口

- `pmAlloc()`：分配一个物理页，返回物理页的地址
- `pmPageIncRef(Page*)`：增加物理页的引用计数
- `pmPageDecRef(Page*)`：减少物理页的引用计数，如果引用计数为 0，则将物理页加入空闲链表

## 虚拟内存管理

虚拟内存管理模块负责管理与页表映射相关的内容。FarmOS 使用 Sv39 虚拟内存管理。

FarmOS 虚拟内存采用两种分配模式。
- `kvmAlloc` 和 `kvmFree`
	- 常用于无映射、不共享的内存分配
	- 调用者需保证成对使用
- `vmAlloc`、`ptMap` 和 `ptUnmap`
	- 常用于有映射的内存分配
	- 调用者需保证使用 `vmAlloc` 后，使用 `ptMap` 添加映射
	- 在使用 `ptUnmap` 解除映射后自动释放内存

### 虚拟内存管理模块的初始化

虚拟内存管理模块的初始化函数为 `vmmInit()`，主要功能为初始化内核页表映射，流程如下：
- 初始化内核页目录
- 映射 UART 寄存器，用于串口输入输出
- 映射 MMIO 的硬盘寄存器，可读可写
- 映射 RTC 寄存器，可读可写
- 映射 PLIC 寄存器，可读可写
- 映射内核代码段，可读可执行
- 映射内核数据段，可读可写
- 映射 Trampoline 段，可读可执行
- 进行内存测试
在初始化后，便可以使用 `vmAlloc()`、`kvmAlloc()`、`ptLookup()`、`ptMap()` 等函数使用虚拟内存，同时也可以开启页表映射。

```c
void vmmInit() {
	// 第一步：初始化内核页目录
	kernPd = (Pte *)pageToPa(pmAlloc());

	vmInitMap(UART0, UART0, PAGE_SIZE, PTE_R | PTE_W);
	vmInitMap(VIRTIO0, VIRTIO0, PAGE_SIZE, PTE_R | PTE_W);
	vmInitMap(RTC_BASE, RTC_BASE, PAGE_SIZE, PTE_R | PTE_W);
	vmInitMap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

	extern char end_text[];
	vmInitMap(KERNBASE, KERNBASE, (u64)end_text - KERNBASE, PTE_R | PTE_X);

	vmInitMap((u64)end_text, (u64)end_text, pmTop() - (u64)end_text, PTE_R | PTE_W);

	extern char trampoline[];
	vmInitMap(PGROUNDDOWN((u64)trampoline), TRAMPOLINE, PAGE_SIZE, PTE_R | PTE_X);

	memoryTest();
}
```

### 虚拟内存管理模块接口

- `kvmAlloc()`：分配一个虚拟页，返回虚拟页的地址，并将对应的物理页的引用计数加一
- `kvmFree()`：释放一个虚拟页，将对应的物理页的引用计数减一

- `vmAlloc()`：分配一个虚拟页，返回虚拟页的地址，需要调用者保证使用 `ptMap()` 建立映射
- `ptMap(Pte *pgdir, u64 va, u64 pa, u64 perm)`：在页表 `pgdir` 中建立虚拟地址 `va` 到物理地址 `pa` 的映射，权限为 `perm`
- `ptUnmap(Pte *pgdir, u64 va)`：在页表 `pgdir` 中取消虚拟地址 `va` 的映射
- `ptLookup(Pte *pgdir, u64 va)`：在页表 `pgdir` 中查找虚拟地址 `va` 对应的页表项，返回页表项

## 硬件抽象层

- `tlbFlush()`：刷新 TLB
- `ptFetch()`：获取当前页目录






## 内存管理工具

FarmOS 对其它模块的内存操作进行了部分抽象和封装，提供了遍历页表的工具函数，位于 `kern/mm/vmtools.c`。

以下是遍历页表工具函数的接口：

```c
typedef err_t (*pte_callback_t)(Pte *pd, u64 target_va, Pte *target_pte, void *arg);
typedef err_t (*pt_callback_t)(Pte *pd, Pte *target_pt, u64 ptlevel, void *arg);

err_t pdWalk(Pte *pd, pte_callback_t pte_callback, pt_callback_t pt_callback, void *arg);
```

在需要遍历页表时，只需要重写所需的回调函数，在回调函数中实现所需的功能，而不需要关心页表的结构以及如何遍历。遍历页表统一使用这部分函数使得代码被重用，提高了代码的可读性和可维护性，也降低了其它模块自行遍历页表时可能出现错误的情况。

以下是在 FarmOS 中的两处实际用例：

- `killProc()`
	- `pdWalk(proc->pageTable, vmUnmapper, kvmUnmapper, NULL);`
	- 在销毁进程时，需要遍历进程的页表，将页表中的映射解除，回收进程使用的页表物理页，解引用进程的数据段、代码段等。
- `procFork()`
	- `pdWalk(proc->pageTable, dupPageCallback, NULL, child->pageTable);`
	- 在进行 `fork` 时，需要遍历进程的页表，加写时复制保护。

