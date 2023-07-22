#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/proc.h>
#include <proc/thread.h>

typedef err_t (*user_kernel_callback_t)(void *uptr, void *kptr, size_t len, void *arg);

/**
 * @brief 用户态到内核态的数据操作
 * @param callback 负责复制数据的函数，返回非 0 表示停止复制
 * @param cow 如果为 1，表示需要对用户地址 uptr 进行写时复制
 */
static void userToKernel(Pte *upd, u64 uptr, void *kptr, size_t len,
			 user_kernel_callback_t callback, bool cow, void *arg) {
	u64 uoff = uptr % PAGE_SIZE;
	int r;

	for (u64 i = 0; i < len; uoff = 0) {
		u64 va = uptr + i;
		Pte pte = ptLookup(upd, va);
		u64 urealpa;

		if (cow && (pte & PTE_COW)) {
			warn("COW when copyOut: va=%lx\n", va);
			// 写时复制
			u64 newpa = vmAlloc();
			u64 oldpa = pteToPa(pte);
			memcpy((void *)newpa, (void *)oldpa, PAGE_SIZE);
			u64 newperm = (PTE_PERM(pte) & ~PTE_COW) | PTE_W;
			if ((r = ptMap(upd, va, newpa, newperm)) < 0) {
				panic("userToKernel: ptMap failed: %d\n", r);
			}
			urealpa = newpa;
		} else {
			urealpa = pteToPa(pte);
		}

		size_t ulen = MIN(len - i, PAGE_SIZE - uoff);
		if (callback((void *)(urealpa + uoff), kptr + i, ulen, arg)) {
			break;
		}
		i += ulen;
	}
}

err_t copyOutCallback(void *uptr, void *kptr, size_t len, void *arg) {
	memcpy(uptr, kptr, len);
	return 0;
}

err_t copyInCallback(void *uptr, void *kptr, size_t len, void *arg) {
	memcpy(kptr, uptr, len);
	return 0;
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

err_t copyInStrCallback(void *uptr, void *kptr, size_t len, void *arg) {
	return strncpyJudgeEnd(kptr, uptr, len);
}

/**
 * @brief 将内核的数据拷贝到用户态地址，使用传入的用户态页表
 */
void copyOutOnPageTable(Pte *pgDir, u64 uPtr, void *kPtr, int len) {
	userToKernel(pgDir, uPtr, kPtr, len, copyOutCallback, true, NULL);
}

/**
 * @brief 将内核的数据拷贝到用户态地址，默认使用当前进程的页表去查询uPtr的物理地址
 */
void copyOut(u64 uPtr, void *kPtr, int len) {
	Pte *pgDir = cur_proc_pt();
	copyOutOnPageTable(pgDir, uPtr, kPtr, len);
}

/**
 * @brief 将用户的数据拷贝入内核
 */
void copyIn(u64 uPtr, void *kPtr, int len) {
	userToKernel(cur_proc_pt(), uPtr, kPtr, len, copyInCallback, false, NULL);
}

/**
 * @brief 将用户态的字符串拷贝进内核
 * @param n 表示传输的最大字符数
 */
void copyInStr(u64 uPtr, void *kPtr, int n) {
	Pte *pgDir = cur_proc_pt();
	userToKernel(pgDir, uPtr, kPtr, n, copyInStrCallback, false, NULL);
}

void copy_in(Pte *upd, u64 uptr, void *kptr, size_t len) {
	userToKernel(upd, uptr, kptr, len, copyInCallback, false, NULL);
}

void copy_in_str(Pte *upd, u64 uptr, void *kptr, size_t len) {
	userToKernel(upd, uptr, kptr, len, copyInStrCallback, false, NULL);
}

void copy_out(Pte *upd, u64 uptr, void *kptr, size_t len) {
	userToKernel(upd, uptr, kptr, len, copyOutCallback, true, NULL);
}
