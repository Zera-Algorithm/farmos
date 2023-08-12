#ifndef _MMU_H_
#define _MMU_H_

#include <types.h>

/**
 * @brief 激活虚拟内存映射
 */
void vmEnable();

/**
 * @brief 清空 TLB
 */
void tlbFlush(u64 va);

/**
 * 获取当前 SATP 寄存器中的页表基址
 */
Pte *ptFetch();

#endif // _MMU_H_
