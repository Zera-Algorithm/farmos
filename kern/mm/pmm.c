/**
 * @author Raspstudio <hzy_he@buaa.edu.cn>
 * @note 物理内存管理相关内容
 */

#include <dev/dtb.h>
#include <fs/buf.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/string.h>
#include <mm/pmm.h>
#include <param.h>
#include <proc/proc.h>
#include <proc/thread.h>
#include <fs/dirent.h>
#include <signal/signal.h>

struct Page {
	u32 ref;
	LIST_ENTRY(Page) link;
};

Page *pages = NULL;

struct pm_allocator {
	PageList pma_freelist;
	u64 pma_bottom; // 页分配器底部接下来可用的 pa
	u64 pma_top; // 页分配器顶部已使用的 pa
	u64 pma_maxpa; // 页分配器最大可用的 pa
	u32 pma_npage;
} pma;

// 分配器接口函数

/**
 * @brief 上推分配器底部内存，调用者需要维护页面结构体引用计数
 */
inline void *pma_rawpushbottom(u64 size) {
	u64 start = pma.pma_bottom;
	pma.pma_bottom += PGROUNDUP(size);
	panic_on(pma.pma_bottom >= pma.pma_top);
	return memset((void *)start, 0, size);
}

/**
 * @brief 上推分配器底部内存，公开接口
 */
inline void *pma_pushup(u64 size) {
	for (u64 off = 0; off < size; off += PAGE_SIZE) {
		u64 pa = pma.pma_bottom + off;
		Page *pp = paToPage(pa);
		pp->ref = 1;
	}
	return pma_rawpushbottom(size);
}

/**
 * @brief 分配器顶部内存已全部被使用，需要下拉分配器顶部内存并返回
 */
inline Page *pma_pulldown() {
	if (pma.pma_bottom == pma.pma_top) {
		error("FarmOS Out of Memory!");
	}
	pma.pma_bottom -= PAGE_SIZE;
	memset((void *)pma.pma_bottom, 0, PAGE_SIZE);
	return paToPage(pma.pma_bottom);
}

/**
 * @brief 回收给定的页面结构体，调用者需要保证传入的页面结构体引用计数为 0
 */
inline void pma_recycle(Page *pp) {
	assert(pp->ref == 0);
	u64 pa = pageToPa(pp);
	assert(pa >= pma.pma_top);
	// 尝试回拉分配器顶部内存
	if (pa == pma.pma_top) {
		// 触发分配器回拉
		pma.pma_top += PAGE_SIZE;
		// 尝试继续回拉空闲页面链表中的内存
		pp = paToPage(pma.pma_top);
		while (pp->ref == 0) {
			LIST_REMOVE(pp, link);
			pma.pma_top += PAGE_SIZE;
			pp = paToPage(pma.pma_top);
		}
	} else {
		// 直接回收
		LIST_INSERT_HEAD(&pma.pma_freelist, pp, link);
	}
}

/**
 * @brief 分配一个空闲物理页清空并返回，调用者需要维护页面结构体引用计数
 */
inline Page *pma_alloc() {
	Page *pp = LIST_FIRST(&pma.pma_freelist);
	if (pp == NULL) {
		return pma_pulldown();
	} else {
		LIST_REMOVE(pp, link);
		memset((void *)pageToPa(pp), 0, PAGE_SIZE);
		return pp;
	}
}

// 模块初始化函数

void pmmInit() {
	// 第一部分：初始化分配器
	extern struct MemInfo memInfo;
	extern char end[];
	pma.pma_maxpa = MEMBASE + PGROUNDDOWN(memInfo.size);
	pma.pma_npage = pma.pma_maxpa / PAGE_SIZE;  // 内存页数
	pma.pma_top = pma.pma_maxpa; // 空闲内存页的结束地址
	pma.pma_bottom = PGROUNDUP((u64)end); // 空闲内存页的起始地址
	LIST_INIT(&pma.pma_freelist); // 初始化空闲链表
	log(MM_GLOBAL, "Physical Memory Avail: [%0lx, %0lx)\n", pma.pma_bottom, pma.pma_top);

	// 第二部分：初始化内存页数组
	pages = pma_rawpushbottom(pma.pma_npage * sizeof(Page)); // 初始化内存页数组
	u64 avail = pageToPpn(paToPage(pma.pma_bottom));
	for (size_t i = 0; i < avail; i++) {
		pages[i].ref = 1;
	}
	log(MM_GLOBAL, "Physical Memory Allocator Now Available: [%0lx, %0lx)\n", pma.pma_bottom, pma.pma_top);

	// Legacy：初始化各种数组
	#define pmInitPush(dummy1, size, dummy2) pma_pushup(size)

	// 进程管理模块的数组
	extern proc_t *procs;
	procs = pmInitPush(freemem, NPROC * sizeof(proc_t), &freemem);
	extern thread_t *threads;
	threads = pmInitPush(freemem, NTHREAD * sizeof(thread_t), &freemem);
	extern void *sigactions;
	sigactions = pmInitPush(freemem, NPROCSIGNALS * NPROC * sizeof(sigaction_t), &freemem);
	extern void *sigevents;
	sigevents = pmInitPush(freemem, NSIGEVENTS * sizeof(sigevent_t), &freemem);

	// 为 VirtIO 驱动分配连续的两页
	extern void *virtioDriverBuffer;
	virtioDriverBuffer = pmInitPush(freemem, 2 * PAGE_SIZE, &freemem);

	// 为磁盘缓存分配内存
	extern void *bufferData;
	bufferData = pmInitPush(freemem, BGROUP_NUM * sizeof(BufferDataGroup), &freemem);
	extern void *bufferGroups;
	bufferGroups = pmInitPush(freemem, BGROUP_NUM * sizeof(BufferGroup), &freemem);
	extern thread_t *threads;
	threads = pmInitPush(freemem, NTHREAD * sizeof(thread_t), &freemem);
	extern proc_t *procs;
	procs = pmInitPush(freemem, NPROC * sizeof(proc_t), &freemem);
	extern void *sigactions;
	sigactions = pmInitPush(freemem, NPROCSIGNALS * NPROC * sizeof(sigaction_t), &freemem);
	extern void *sigevents;
	sigevents = pmInitPush(freemem, NSIGEVENTS * sizeof(sigevent_t), &freemem);
	extern Dirent *dirents;
	dirents = pmInitPush(freemem, MAX_DIRENT * sizeof(Dirent), &freemem);

	// 为内核栈分配内存
	extern void *kstacks;
	kstacks = pmInitPush(freemem, NPROC * TD_KSTACK_PAGE_NUM * PAGE_SIZE, &freemem);

	log(MM_GLOBAL, "Physical Memory Init Finished, `pm` Functions Available!\n");
}

// 纯接口函数

inline u64 pageToPpn(Page *p) {
	return p - pages;
}

inline u64 pageToPa(Page *p) {
	return MEMBASE + (pageToPpn(p) << PAGE_SHIFT);
}

inline Page *paToPage(u64 pa) {
	return &pages[(pa - MEMBASE) >> PAGE_SHIFT];
}

inline u64 pmTop() {
	return pma.pma_maxpa;
}

u64 pm_freemem() {
	return pma.pma_top - pma.pma_bottom;
}

// 功能接口函数

Page *pmAlloc() {
	return pma_alloc();
}

Page *pm_push(u64 size) {
	return pma_pushup(size);
}

void pmPageIncRef(Page *pp) {
	panic_on(pp == NULL);
	pp->ref++;
}

void pmPageDecRef(Page *pp) {
	panic_on(pp == NULL || pp->ref == 0);
	pp->ref--;
	if (pp->ref == 0) {
		pma_recycle(pp);
	}
}
