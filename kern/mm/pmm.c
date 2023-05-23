/**
 * @author Raspstudio <hzy_he@buaa.edu.cn>
 * @note 物理内存管理相关内容
 */

#include <dev/dtb.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <mm/pmm.h>

struct Page {
	u32 ref;
	PageLink link;
};

u64 npage = 0;
Page *pages = NULL;
PageList pageFreeList;

extern struct MemInfo memInfo;
extern char end[];

// 模块初始化函数

static void *pmInitPush(u64 start, u64 size, u64 *freemem) {
	u64 push = PGROUNDUP(size);
	*freemem = start + push;
	loga("\tPhysical Memory Used: 0x%08x~0x%08x\n", start, *freemem);
	return memset((void *)start, 0, push);
}

void pmmInit() {
	// 第一部分：读取设备信息，获取内存信息，初始化内存页数组
	loga("Physical Memory Init Start: End = 0x%08x\n", end);
	u64 freemem = PGROUNDUP((u64)end); // 空闲内存页的起始地址
	npage = memInfo.size / PAGE_SIZE;  // 内存页数
	pages = pmInitPush(freemem, npage * sizeof(Page), &freemem); // 初始化内存页数组

	// 第二部分：初始化空闲链表
	loga("Physical Memory Freelist Init Start: Freemem = 0x%08x\n", freemem);
	LIST_INIT(&pageFreeList);
	u64 pageused = (freemem - MEMBASE) >> PAGE_SHIFT; // 已经使用的内存页数
	for (u64 i = 0; i < pageused; i++) {
		pages[i].ref = 1;
	}
	loga("\tTo pages[0:%d) used\n", pageused);
	for (u64 i = pageused; i < npage; i++) {
		LIST_INSERT_HEAD(&pageFreeList, &pages[i], link);
	}
	loga("\tFrom pages[%d:] free\n", pageused);

	loga("Physical Memory Init Finished, `pm` Functions Available!\n");
}

// 纯接口函数

inline u64 __attribute__((warn_unused_result)) pageToPpn(Page *p) {
	return p - pages;
}

inline u64 __attribute__((warn_unused_result)) pageToPa(Page *p) {
	return MEMBASE + (pageToPpn(p) << PAGE_SHIFT);
}

inline Page *__attribute__((warn_unused_result)) paToPage(u64 pa) {
	return &pages[(pa - MEMBASE) >> PAGE_SHIFT];
}

inline u64 __attribute__((warn_unused_result)) pmTop() {
	return MEMBASE + (npage << PAGE_SHIFT);
}

// 功能接口函数

Page *__attribute__((warn_unused_result)) pmAlloc() {
	Page *pp = LIST_FIRST(&pageFreeList);
	panic_on(pp == NULL);
	// 从空闲页链表中移除该页
	LIST_REMOVE(pp, link);
	// 清空页面内容并返回
	memset((void *)pageToPa(pp), 0, PAGE_SIZE);
	loga("\tAlloc pm-page: %d\n", pageToPpn(pp));
	return pp;
}

void pmPageIncRef(Page *pp) {
	panic_on(pp == NULL);
	pp->ref++;
}

void pmPageDecRef(Page *pp) {
	panic_on(pp == NULL || pp->ref == 0);
	pp->ref--;
	if (pp->ref == 0) {
		LIST_INSERT_HEAD(&pageFreeList, pp, link);
	}
}
