# FarmOS 内存管理



## 概述

FarmOS 的内存管理分为两部分，物理内存管理和虚拟内存管理。主要相关文件目录结果如下：
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
	- `pmm_init()`：物理内存管理初始化
	- `vmm_init()`：虚拟内存管理初始化
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

物理内存管理模块的初始化函数为 `pmm_init()`，主要功能如下：
- 读取设备信息，获取内存信息，初始化内存页数组
- 为需要连续内存空间的模块分配内存
- 初始化空闲页面链表
在初始化后，便可以使用 `pm_alloc()`、`pm_page_inc_ref()`、`pm_page_dec_ref()` 函数使用物理内存。

```c
void pmm_init() {
	// 第一部分：读取设备信息，获取内存信息，初始化内存页数组
	u64 freemem = PGROUNDUP((u64)end); // 空闲内存页的起始地址
	npage = memInfo.size / PAGE_SIZE;  // 内存页数
	pages = pm_initpush(freemem, npage * sizeof(Page), &freemem); // 初始化内存页数组

	// 第二部分：为需要连续内存空间的模块分配内存
	extern void *example;
	example = pm_initpush(freemem, /* Size */, &freemem);
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

- `pm_alloc()`：分配一个物理页，返回物理页的地址
- `pm_page_inc_ref(Page*)`：增加物理页的引用计数
- `pm_page_dec_ref(Page*)`：减少物理页的引用计数，如果引用计数为 0，则将物理页加入空闲链表

## 虚拟内存管理

虚拟内存管理模块负责管理与页表映射相关的内容。FarmOS 使用 Sv39 虚拟内存管理。

FarmOS 虚拟内存采用两种分配模式。
- `kvm_alloc` 和 `kvm_free`
	- 常用于无映射、不共享的内存分配
	- 调用者需保证成对使用
- `vm_alloc`、`pt_map` 和 `pt_unmap`
	- 常用于有映射的内存分配
	- 调用者需保证使用 `vm_alloc` 后，使用 `pt_map` 添加映射
	- 在使用 `pt_unmap` 解除映射后自动释放内存

### 虚拟内存管理模块的初始化

虚拟内存管理模块的初始化函数为 `vmm_init()`，主要功能为初始化内核页表映射，流程如下：
- 初始化内核页目录
- 映射 UART 寄存器，用于串口输入输出
- 映射 MMIO 的硬盘寄存器，可读可写
- 映射 RTC 寄存器，可读可写
- 映射 PLIC 寄存器，可读可写
- 映射内核代码段，可读可执行
- 映射内核数据段，可读可写
- 映射 Trampoline 段，可读可执行
- 进行内存测试
在初始化后，便可以使用 `vm_alloc()`、`kvm_alloc()`、`pt_lookup()`、`pt_map()` 等函数使用虚拟内存，同时也可以开启页表映射。

```c
void vmm_init() {
	// 第一步：初始化内核页目录
	kernPd = (pte_t *)page_to_pa(pm_alloc());

	vm_initmap(UART0, UART0, PAGE_SIZE, PTE_R | PTE_W);
	vm_initmap(VIRTIO0, VIRTIO0, PAGE_SIZE, PTE_R | PTE_W);
	vm_initmap(RTC_BASE, RTC_BASE, PAGE_SIZE, PTE_R | PTE_W);
	vm_initmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

	extern char end_text[];
	vm_initmap(KERNBASE, KERNBASE, (u64)end_text - KERNBASE, PTE_R | PTE_X);

	vm_initmap((u64)end_text, (u64)end_text, pmTop() - (u64)end_text, PTE_R | PTE_W);

	extern char trampoline[];
	vm_initmap(PGROUNDDOWN((u64)trampoline), TRAMPOLINE, PAGE_SIZE, PTE_R | PTE_X);
}
```

### 虚拟内存管理模块接口

- `kvm_alloc()`：分配一个虚拟页，返回虚拟页的地址，并将对应的物理页的引用计数加一
- `kvm_free()`：释放一个虚拟页，将对应的物理页的引用计数减一

- `vm_alloc()`：分配一个虚拟页，返回虚拟页的地址，需要调用者保证使用 `pt_map()` 建立映射
- `pt_map(pte_t *pgdir, u64 va, u64 pa, u64 perm)`：在页表 `pgdir` 中建立虚拟地址 `va` 到物理地址 `pa` 的映射，权限为 `perm`
- `pt_unmap(pte_t *pgdir, u64 va)`：在页表 `pgdir` 中取消虚拟地址 `va` 的映射
- `pt_lookup(pte_t *pgdir, u64 va)`：在页表 `pgdir` 中查找虚拟地址 `va` 对应的页表项，返回页表项

## 写时复制与共享内存机制

### 写时复制机制

FarmOS 使用写时复制机制来实现进程的 fork 操作，以充分利用内存。我们在页表项上使用了 1 位软件保留位，标记其为写时复制页面。我们保证对于写时复制页面没有写入权限，即 `PTE_COW` 和 `PTE_W` 标志不同时存在。

在进程 fork 时，我们会遍历父进程的页表，对于每一个有效的页表项进行如下操作。
- 对于不可写的页面，直接复制其页表项，保留原有的权限
- 对于可写非共享的页面，将其标记为写时复制页面，同时将其权限修改为只读

```c
static err_t duppage(pte_t *pd, u64 target_va, pte_t *target_pte, void *arg) {
	pte_t *childpd = (pte_t *)arg;
	// 跳过 Trapframe/Trampoline（已在新内核线程中映射过，不需要再映射）
	if (pt_lookup(childpd, target_va) == 0) {
		// 从父线程的页表中获取映射信息
		pte_t parentpte = pt_lookup(pd, target_va);
		u64 perm = PTE_PERM(parentpte);
		// 如果父线程的页表项是用户可写的，则进行写时复制
		if (PTE_PASSIVE(perm)) {
			return pt_map(childpd, target_va, 0, perm);
		} else if ((perm & PTE_W) && (perm & PTE_U) && !(perm & PTE_SHARED)) {
			// 用户态非共享可写页，进行写时复制
			perm = (perm & ~PTE_W) | PTE_COW;
			return pt_map(childpd, target_va, pteToPa(parentpte), perm) ||
			       pt_map(pd, target_va, pteToPa(parentpte), perm);
		} else if (perm & PTE_U) {
			return pt_map(childpd, target_va, pteToPa(parentpte), perm);
		} else {
			error("duppage: invalid perm %x\n", perm);
		}
	}
	return 0;
}
``` 

这样一来，在用户尝试写入写时复制页面时，便会触发异常，从而进入我们的异常处理程序。在异常处理程序中，我们会将写时复制页面复制一份，同时修改权限，使其可写，从而实现写时复制。

### 共享内存机制

由于进程可能会需要使用共享内存（即 `shm` 相关操作），因此我们使用了 1 位软件保留位实现共享内存操作。在进程 fork/clone 时，会遍历页表，对于共享内存的部分会进行复制映射（调用 `duppage` 函数同前）。

## 被动分配机制

由于系统内存有限，用户在申请内存后并不一定会使用到全部内存，因此我们引入了被动分配机制。由于我们既需要保证用户不操作非法的页面，又需要记录用户希望对该界面映射的权限，因此我们使用页表项遵守如下使用规范。

在 FarmOS 中，页表项有三种状态：
- 有效页面（`PTE_V` 为高）：页表项指向一个有效的物理页
- 被动有效页面（`PTE_V` 为低，`PTE_U` 为高）：页表项对应的虚拟地址已被用户申请，但是尚未映射到物理页（此时页表项的物理页号部分为零）
- 无效页面（`PTE_V` 为低，页表项为全零）：页表项无效

```c
/**
 * 对于页表项的 3 种状态间转换（有效、被动有效、无效）：
 * 1. 有效 -> 有效：
 * 		修改页表项内容（分支 1）
 * 2. 有效 -> 被动有效：
 * 		不应该存在这种情况
 * 3. 有效 -> 无效：
 * 		不应该存在这种情况，应该使用 pt_unmap
 * 4. 被动有效 -> 有效：
 * 		分配页面，修改页表项内容（分支 3）
 * 5. 被动有效 -> 被动有效：
 * 		修改页表项内容，更新权限（分支 2）
 * 6. 被动有效 -> 无效：
 * 		不应该存在这种情况，应该使用 pt_unmap
 * 7. 无效 -> 有效：
 * 		分配页面，修改页表项内容（分支 3）
 * 8. 无效 -> 被动有效：
 * 		修改页表项内容，更新权限（分支 2）
 * 9. 无效 -> 无效：
 * 		这是在干什么？
 */
err_t pt_map(pte_t *pgdir, u64 va, u64 pa, u64 perm);
```

我们会在以下几种情况下使用被动分配：
- 映射用户栈：使用被动分配来实现可拓展的用户栈而不占用过多内存
- 用户堆内存：使用被动分配来为用户拓展堆空间
- 用户匿名映射：使用被动分配来实现 `mmap()` 的匿名映射

这样一来，我们就可以在用户申请内存时，不分配实际的物理页，从而节省内存。当用户第一次访问该虚拟地址时，会触发异常，从而进入我们的异常处理程序。在异常处理程序中，我们会为用户分配一个物理页，并建立映射，从而实现被动分配。

为了提高被动分配的效率，我们也在添加映射时做了判断，对于用户一部分堆空间内的区域，我们会一次性分配好物理页，从而减少异常处理的次数。


## 硬件抽象层

- `tlb_flush()`：刷新 TLB
- `pt_fetch()`：获取当前页目录


## 内存管理工具

FarmOS 对其它模块的内存操作进行了部分抽象和封装，提供了遍历页表的工具函数，位于 `kern/mm/vmtools.c`。

以下是遍历页表工具函数的接口：

```c
typedef err_t (*pte_callback_t)(pte_t *pd, u64 target_va, pte_t *target_pte, void *arg);
typedef err_t (*pt_callback_t)(pte_t *pd, pte_t *target_pt, u64 ptlevel, void *arg);

err_t pdWalk(pte_t *pd, pte_callback_t pte_callback, pt_callback_t pt_callback, void *arg);
```

在需要遍历页表时，只需要重写所需的回调函数，在回调函数中实现所需的功能，而不需要关心页表的结构以及如何遍历。遍历页表统一使用这部分函数使得代码被重用，提高了代码的可读性和可维护性，也降低了其它模块自行遍历页表时可能出现错误的情况。



## 内核空间到用户空间的数据传输

由于我们经常会进行内核空间到用户空间的数据传输，同时又因为我们设计了被动调页、写时复制机制，因此我们设计了一套内核空间到用户空间的数据传输机制。

我们设计了一套高可拓展性的数据拷贝机制，将用户态的页面异常处理函数接入，会自动处理被动调页、写时复制等情况，从而实现了高效的内核空间到用户空间的数据传输。

通过 `user_to_kernel` 函数，可以根据用户空间与内核空间的映射调用用户传入的回调函数。用户在调用时传入本次操作所需的权限，在权限不足时，会自动触发对应的异常处理函数，解决可能存在的写时复制或被动调页问题。

```c
typedef err_t (*user_kernel_callback_t)(void *uptr, void *kptr, size_t len, void *arg);
static void user_to_kernel(pte_t *upd, u64 uptr, void *kptr, size_t len,
			 user_kernel_callback_t callback, u64 permneed, void *arg) {
	u64 uoff = uptr % PAGE_SIZE;

	for (u64 i = 0; i < len; uoff = 0) {
		u64 va = uptr + i;
		pte_t pte = pt_lookup(upd, va);
		
		test_page_fault(upd, va, pte, permneed);

		u64 urealpa = pte_to_Pa(pt_lookup(upd, va));
		size_t ulen = MIN(len - i, PAGE_SIZE - uoff);
		if (callback((void *)(urealpa + uoff), kptr + i, ulen, arg)) {
			break;
		}
		i += ulen;
	}
}
```

基于以上机制，我们实现了以下几个接口函数供内核使用。

```c
void copy_in(pte_t *upd, u64 uptr, void *kptr, size_t len);
void copy_in_str(pte_t *upd, u64 uptr, void *kptr, size_t len);
void copy_out(pte_t *upd, u64 uptr, void *kptr, size_t len);
```

