#include "mm/memlayout.h"
#include "mm/memory.h"
#include "param.h"
#include "proc/proc.h"
#include "riscv.h"
#include "types.h"
#include <lib/queue.h>
#include <lib/string.h>
#include <proc/proc.h>
#include <trap/trap.h>

struct Proc procs[NPROC];
// 空闲进程链表
struct ProcFreeList procFreeList;
// 调度进程队列
struct ProcSchedQueue procSchedQueue[NCPU];

/**
 * @brief 初始化空闲进程链表和调度进程队列
 */
void procInit() {
	LIST_INIT(&procFreeList);
	for (int i = 0; i < NCPU; i++) {
		TAILQ_INIT(&procSchedQueue[i]);
	}

	// 按逆序插入，保持进程在队列中顺序排放
	for (int i = NPROC - 1; i >= 0; i--) {
		LIST_INSERT_HEAD(&procFreeList, &procs[i], procFreeLink);
	}
}
