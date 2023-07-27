#include <lib/error.h>
#include <lib/log.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lock/mutex.h>
#include <mm/kmalloc.h>
#include <mm/memlayout.h>
#include <mm/mmu.h>
#include <mm/vmm.h>

static u64 heap_top = KERNEL_MALLOC;

mutex_t mtx_kmalloc;

static malloc_config_t malloc_config[] = {
    {.size = 64, .npage = 40},
    {.size = 128, .npage = 40},
    {.size = 256, .npage = 80},
    {.size = 512, .npage = 80},
    {.size = 1024, .npage = 40},
    {.size = 2048, .npage = 40},
	{.size = 4096, .npage = 0},
    {.size = -1},
};

static inline void kpage_alloc(u64 va) {
	extern pte_t *kernPd;
	u64 pa = vmAlloc();
	panic_on(ptMap(kernPd, va, pa, PTE_R | PTE_W));
}

static inline void list_insert_head(malloc_header_t **phead, malloc_header_t *node) {
	node->next = *phead;
	*phead = node;
}

/**
 * @brief 初始化链表指针
 */
void kmalloc_init() {
	// 1. 分配内存
	u64 npage = 0;
	for (int i = 0; malloc_config[i].size != -1; i++) {
		npage += malloc_config[i].npage;
	}
	for (int i = 0; i < npage; i++) {
		kpage_alloc(heap_top + i * PAGE_SIZE);
	}

	// 2. 初始化链表
	for (int i = 0; malloc_config[i].size != -1; i++) {
		malloc_config[i].accual_size = malloc_config[i].size - sizeof(malloc_header_t);
		malloc_config[i].head = NULL;

		// j指向在本大小对象池中的偏移
		for (int j = 0; j < malloc_config[i].npage * PAGE_SIZE;
		     j += malloc_config[i].size) {
			malloc_header_t *header = (malloc_header_t *)(heap_top + j);
			header->phead = &malloc_config[i].head;
			header->next = NULL;
			header->size = malloc_config[i].accual_size;
			list_insert_head(&malloc_config[i].head, header);
		}

		// 增大heap_top
		heap_top += malloc_config[i].npage * PAGE_SIZE;
	}

	// 3. 初始化锁
	mtx_init(&mtx_kmalloc, "kmalloc", true, MTX_SPIN);
}

/**
 * @brief 扩展堆，为给定的对象类型扩展一页空间
 */
static void extend_heap(malloc_config_t *config) {
	// 1. 分配内存
	kpage_alloc(heap_top);

	// 2. 如果超过了malloc的最大分配页数，就panic
	if (heap_top + PAGE_SIZE > KERNEL_MALLOC + MAX_MALLOC_NPAGE * PAGE_SIZE) {
		panic("kmalloc: out of memory\n");
	}

	// 记录对象池的内存增加
	config->npage += 1;

	// 2. 向链表中添加新分配的对象
	for (int i = 0; i < PAGE_SIZE; i += config->size) {
		malloc_header_t *header = (malloc_header_t *)(heap_top + i);
		header->phead = &config->head;
		header->next = NULL;
		header->size = config->accual_size;
		list_insert_head(&config->head, header);
	}

	// 3. 增大heap_top
	heap_top += PAGE_SIZE;
}

void *kmalloc(size_t size) {
	mtx_lock(&mtx_kmalloc);

	// 1. 找到合适的大小
	int i;
	for (i = 0; malloc_config[i].size != -1; i++) {
		if (malloc_config[i].accual_size >= size) {
			break;
		}
	}
	if (malloc_config[i].size == -1) {
		panic("kmalloc: size %ld is too large\n", size);
	}

	// 2. 从链表中取出
	malloc_header_t *header = malloc_config[i].head;
	if (header == NULL) {
		warn("kalloc: object of size %d is used up, try to extend a page\n",
		     malloc_config[i].size);
		extend_heap(&malloc_config[i]);
		header = malloc_config[i].head;
	}
	malloc_config[i].head = header->next;
	// TODO: 需要判断header+1的内存地址是否紧跟着header
	assert((void *)(header + 1) == (void *)header + sizeof(malloc_header_t));

	// 3. 清空分配区域的内存
	void *addr = (void *)(header + 1);
	memset(addr, 0, header->size);

	mtx_unlock(&mtx_kmalloc);
	return addr;
}

void kfree(void *ptr) {
	mtx_lock(&mtx_kmalloc);

	malloc_header_t *header = (malloc_header_t *)(ptr - sizeof(malloc_header_t));
	list_insert_head(header->phead, header);

	mtx_unlock(&mtx_kmalloc);
}
