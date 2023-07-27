#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/proc.h>
#include <proc/thread.h>
#include <trap/trap.h>

typedef err_t (*user_kernel_callback_t)(void *uptr, void *kptr, size_t len, void *arg);


extern err_t page_fault_handler(pte_t *pd, u64 violate, u64 badva);
static void test_page_fault(Pte *upd, u64 va, Pte pte, u64 permneed) {
	// 无效，传入权限一定违反
	if (!(pte & PTE_V)) {
		page_fault_handler(upd, permneed, va);
	}
	// 有效，检查是否违反各项权限
	if ((permneed & PTE_W) && !(pte & PTE_W)) {
		panic_on(page_fault_handler(upd, PTE_W, va));
	}
	if ((permneed & PTE_R) && !(pte & PTE_R)) {
		panic_on(page_fault_handler(upd, PTE_R, va));
	}
}

/**
 * @brief 用户态到内核态的数据操作
 * @param callback 负责复制数据的函数，返回非 0 表示停止复制
 * @param cow 如果为 1，表示需要对用户地址 uptr 进行写时复制
 */
static void userToKernel(Pte *upd, u64 uptr, void *kptr, size_t len,
			 user_kernel_callback_t callback, u64 permneed, void *arg) {
	u64 uoff = uptr % PAGE_SIZE;

	for (u64 i = 0; i < len; uoff = 0) {
		u64 va = uptr + i;
		Pte pte = ptLookup(upd, va);
		
		test_page_fault(upd, va, pte, permneed);

		u64 urealpa = pteToPa(ptLookup(upd, va));
		// for debug
		if (urealpa < 0x1000 || (u64)kptr < 0x1000) {
			asm volatile("nop");
			panic("address too low: urealpa=%lx, pte=%lx, kptr=%lx\n", urealpa, pte, (u64)kptr);
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
	userToKernel(pgDir, uPtr, kPtr, len, copyOutCallback, PTE_W, NULL);
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
	userToKernel(cur_proc_pt(), uPtr, kPtr, len, copyInCallback, PTE_R, NULL);
}

/**
 * @brief 将用户态的字符串拷贝进内核
 * @param n 表示传输的最大字符数
 */
void copyInStr(u64 uPtr, void *kPtr, int n) {
	Pte *pgDir = cur_proc_pt();
	userToKernel(pgDir, uPtr, kPtr, n, copyInStrCallback, PTE_R, NULL);
}

void copy_in(Pte *upd, u64 uptr, void *kptr, size_t len) {
	userToKernel(upd, uptr, kptr, len, copyInCallback, PTE_R, NULL);
}

void copy_in_str(Pte *upd, u64 uptr, void *kptr, size_t len) {
	userToKernel(upd, uptr, kptr, len, copyInStrCallback, PTE_R, NULL);
}

void copy_out(Pte *upd, u64 uptr, void *kptr, size_t len) {
	userToKernel(upd, uptr, kptr, len, copyOutCallback, PTE_W, NULL);
}
