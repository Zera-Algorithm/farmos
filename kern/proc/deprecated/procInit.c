
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
struct ProcList procFreeList;
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

// ////////////////////////
// static char strBuf[1024];
// /**
//  * @param argv char *[]类型
//  */
// static inline u64 initStack(void *stackTop, u64 argv) {
// 	u64 argUPtr[20]; // TODO: 可以允许更多的参数
// 	u64 argc = 0;
// 	void *stackNow = stackTop;

// 	// 1. 存储 argv 数组里面的字符串
// 	do {
// 		u64 pStr, len;
// 		copyIn(argv, &pStr, sizeof(void *));

// 		// 空指针，表示结束
// 		if (pStr == 0) {
// 			break;
// 		}

// 		copyInStr(pStr, strBuf, 1024);
// 		len = strlen(strBuf);

// 		log(LEVEL_GLOBAL, "passed argv str: %s\n", strBuf);

// 		stackNow -= (len + 1);
// 		safestrcpy(stackNow, strBuf, 1024);

// 		argUPtr[argc++] = USTACKTOP - (stackTop - stackNow);

// 		argv += sizeof(char *); // 读取下一个字符串
// 	} while (1);
// 	argUPtr[argc++] = 0;

// 	// 2. 存储 char * 数组 argv
// 	stackNow -= argc * sizeof(char *);
// 	memcpy(stackNow, argUPtr, argc * sizeof(char *));

// 	argc -= 1;

// 	// 3. 存储argc（argv由用户本地计算）
// 	stackNow -= sizeof(long);
// 	*(u64 *)stackNow = argc;

// 	return USTACKTOP - (stackTop - stackNow);
// }

// /**
//  * @brief 读取一个文件到内核的虚拟内存
//  * @param binary 返回文件的虚拟地址
//  * @param size 返回文件的大小
//  */
// static void loadFileToKernel(Dirent *file, void **binary, int *size) {
// 	int _size;
// 	void *_binary;

// 	*size = _size = file->fileSize;
// 	*binary = _binary = (void *)KERNEL_TEMP;
// 	extern Pte *kernPd; // 内核页表

// 	// 1. 分配足够的页
// 	int npage = (_size) % PAGE_SIZE == 0 ? (_size / PAGE_SIZE) : (_size / PAGE_SIZE + 1);
// 	for (int i = 0; i < npage; i++) {
// 		u64 pa = vmAlloc();
// 		u64 va = ((u64)_binary) + i * PAGE_SIZE;
// 		panic_on(ptMap(kernPd, va, pa, PTE_R | PTE_W));
// 	}

// 	// 2. 读取文件
// 	fileRead(file, 0, (u64)_binary, 0, _size);
// }