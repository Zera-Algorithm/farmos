/**
 * @author Raspstudio <hzy_he@buaa.edu.cn>
 * @note 物理内存管理相关内容
 */

#ifndef _PMM_H
#define _PMM_H

#include <lib/queue.h>
#include <mm/memlayout.h>
#include <types.h>

// 物理页面结构体相关声明
typedef struct Page Page;
typedef LIST_HEAD(PageList, Page) PageList;

void pmmInit();

Page *pmAlloc() __attribute__((warn_unused_result));
Page *pm_push(u64 size) __attribute__((warn_unused_result));
void pmPageIncRef(Page *pp);
void pmPageDecRef(Page *pp);

u64 pmTop() __attribute__((warn_unused_result));
u64 pageToPpn(Page *p) __attribute__((warn_unused_result));
u64 pageToPa(Page *p) __attribute__((warn_unused_result));
Page *paToPage(u64 pa) __attribute__((warn_unused_result));
u64 pm_freemem();

#endif // _PMM_H
