#ifndef _TRANSFER_H
#define _TRANSFER_H
// 内核与用户态数据传输相关函数
#include <mm/vmm.h>
#include <types.h>

void copyOutOnPageTable(Pte *pgDir, u64 uPtr, void *kPtr, int len);
void copyOut(u64 uPtr, void *kPtr, int len);
void copyIn(u64 uPtr, void *kPtr, int len);
void copyInStr(u64 uPtr, void *kPtr, int n);

void copy_in(Pte *upd, u64 uptr, void *kptr, size_t len);
void copy_in_str(Pte *upd, u64 uptr, void *kptr, size_t len);
void copy_out(Pte *upd, u64 uptr, void *kptr, size_t len);

#endif
