#ifndef _MEMORY_H
#define _MEMORY_H

#include <lib/printf.h>
#include <lib/queue.h>
#include <mm/memlayout.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <types.h>

#define catchMemErr(expr)                                                                          \
	do {                                                                                       \
		u64 err = (expr);                                                                  \
		if (err) {                                                                         \
			return err;                                                                \
		}                                                                                  \
	} while (0)

void enablePagingHart();
void initKernelMemory();
void flushTlb();

#endif // _MEMORY_H
