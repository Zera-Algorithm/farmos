#include <lib/printf.h>
#include <lib/string.h>
#include <proc/proc.h>

/**
 * @brief 将内核的数据拷贝到用户态地址，使用传入的用户态页表
 */
void copyOutOnPageTable(Pte *pgDir, u64 uPtr, void *kPtr, int len) {
	Pte pte = ptLookup(pgDir, uPtr);

	void *p = (void *)pteToPa(pte);
	u64 offset = uPtr & (PAGE_SIZE - 1);

	if (offset != 0) {
		memcpy(p + offset, kPtr, MIN(len, PAGE_SIZE - offset));
	}

	u64 i = uPtr + MIN(len, PAGE_SIZE - offset);
	u64 dstVa = uPtr + len;
	for (; i < dstVa; i += PAGE_SIZE) {
		p = (void *)pteToPa(ptLookup(pgDir, i));
		memcpy(p, kPtr + i - uPtr, MIN(dstVa - i, PAGE_SIZE));
	}
}

/**
 * @brief 将内核的数据拷贝到用户态地址，默认使用当前进程的页表去查询uPtr的物理地址
 */
void copyOut(u64 uPtr, void *kPtr, int len) {
	Pte *pgDir = myProc()->pageTable;
	Pte pte = ptLookup(pgDir, uPtr);

	void *p = (void *)pteToPa(pte);
	u64 offset = uPtr & (PAGE_SIZE - 1);

	if (offset != 0) {
		memcpy(p + offset, kPtr, MIN(len, PAGE_SIZE - offset));
	}

	u64 i = uPtr + MIN(len, PAGE_SIZE - offset);
	u64 dstVa = uPtr + len;
	for (; i < dstVa; i += PAGE_SIZE) {
		p = (void *)pteToPa(ptLookup(pgDir, i));
		memcpy(p, kPtr + i - uPtr, MIN(dstVa - i, PAGE_SIZE));
	}
}

/**
 * @brief 将用户的数据拷贝入内核
 */
void copyIn(u64 uPtr, void *kPtr, int len) {
	Pte *pgDir = myProc()->pageTable;

	void *p = (void *)pteToPa(ptLookup(pgDir, uPtr));
	u64 offset = uPtr & (PAGE_SIZE - 1);

	if (offset != 0) {
		memcpy(kPtr, p + offset, MIN(len, PAGE_SIZE - offset));
	}

	u64 i = uPtr + MIN(len, PAGE_SIZE - offset);
	u64 dstVa = uPtr + len;
	for (; i < dstVa; i += PAGE_SIZE) {
		p = (void *)pteToPa(ptLookup(pgDir, i));
		memcpy(kPtr + i - uPtr, p, MIN(dstVa - i, PAGE_SIZE));
	}
}

/**
 * @brief 拷贝最多n个字符，到 0 终止。
 * @return 如果遇到0，返回-1，否则返回0
 */
static int strncpyJudgeEnd(char *s, const char *t, int n) {
	while (n-- > 0 && (*s++ = *t++) != 0) {
		;
	}

	if (*(s - 1) == 0) {
		return -1;
	} else {
		return 0;
	}
}

/**
 * @brief 将用户态的字符串拷贝进内核
 * @param n 表示传输的最大字符数
 */
void copyInStr(u64 uPtr, void *kPtr, int n) {
	Pte *pgDir = myProc()->pageTable;

	// 1. 获取用户态数据的物理地址指针
	void *p = (void *)pteToPa(ptLookup(pgDir, uPtr));
	u64 offset = uPtr & (PAGE_SIZE - 1);

	// 2. 拷贝从offset开始剩余的数据
	if (offset != 0) {
		if (strncpyJudgeEnd(kPtr, p + offset, MIN(n, PAGE_SIZE - offset)) == -1) {
			return;
		}
	}

	// 3. 整页整页地拷贝剩余的部分
	u64 i = uPtr + MIN(n, PAGE_SIZE - offset);
	u64 dstVa = uPtr + n;
	for (; i < dstVa; i += PAGE_SIZE) {
		p = (void *)pteToPa(ptLookup(pgDir, i));
		if (strncpyJudgeEnd(kPtr + i - uPtr, p, MIN(dstVa - i, PAGE_SIZE)) == -1) {
			return;
		}
	}
}
