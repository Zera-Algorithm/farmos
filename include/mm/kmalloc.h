#ifndef _KMALLOC_H
#define _KMALLOC_H
#include <types.h>

// malloc空间的公用头部
typedef struct malloc_header {
	struct malloc_header *next;
	struct malloc_header **phead; // 指向链表头部的指针
	u64 size;
} malloc_header_t;

typedef struct malloc_config {
	int size; // 包含header的size
	malloc_header_t *head;
	int npage;	 // 此部分对象池的默认页数
	int accual_size; // 实际大小（扣除header）
} malloc_config_t;

// malloc最大分配的页数
#define MAX_MALLOC_NPAGE 20000

void kmalloc_init();
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
