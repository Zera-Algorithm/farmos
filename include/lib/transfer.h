#ifndef _TRANSFER_H
#define _TRANSFER_H
// 内核与用户态数据传输相关函数
#include <mm/vmm.h>
#include <types.h>

void copyOutOnPageTable(Pte *pgDir, u64 uPtr, void *kPtr, int len);
void copyOut(u64 uPtr, void *kPtr, int len);
void copyIn(u64 uPtr, void *kPtr, int len);
void copyInStr(u64 uPtr, void *kPtr, int n);

#endif
