#ifndef _MEMORY_H
#define _MEMORY_H

#include <types.h>

void enablePagingHart();
void initKernelMemory();
void flushTlb();

#endif // _MEMORY_H
