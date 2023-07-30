#include <lib/log.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/proc.h>
#include <proc/thread.h>
#include <sys/syscall.h>

#define PASSIVE_THRESHOLD 0x2000000

err_t sys_map(u64 start, u64 len, u64 perm) {
	u64 from = PGROUNDDOWN(start);
	u64 to = PGROUNDUP(start + len - 1);
	pte_t *pt = cur_proc_pt();
	for (u64 va = from; va < to; va += PAGE_SIZE) {
		if (pteToPa(ptLookup(pt, va)) == 0) {
			if (va < PASSIVE_THRESHOLD) {
				// 使用主动调页机制，若对应虚拟地址没有映射则添加主动映射
				panic_on(ptMap(pt, va, vmAlloc(), perm));
			} else {
				// 使用被动调页机制，若对应虚拟地址没有映射则添加被动映射
				panic_on(ptMap(pt, va, 0, PTE_PASSIVE | perm));
			}
		}
	}
	return 0;
}

/**
 * @brief addr若为0，传回当前堆的位置；addr不为0时，将堆的位置设置为addr，返回新堆的位置
 */
err_t sys_brk(u64 addr) {
	int ret;
	thread_t *td = cpu_this()->cpu_running;
	// 打印brk
	log(LEVEL_MODULE, "old_brk: %lx, brk: %lx\n", td->td_brk, addr);

	mtx_lock(&td->td_proc->p_lock);

	u64 cur_brk = td->td_brk;
	if (addr == 0) {
		mtx_unlock(&td->td_proc->p_lock);
		return cur_brk;
	} else if (addr < cur_brk) {
		// 缩短堆
		td->td_brk = addr;
		ret = sys_unmap(addr + 1, cur_brk);
		mtx_unlock(&td->td_proc->p_lock);
		if (ret < 0)
			return ret;
		else
			return addr;
	} else {
		// 伸长堆
		td->td_brk = addr;
		// TODO: 在后续测试中校验边界点的设定是否正确
		ret = sys_map(cur_brk, addr - cur_brk, PTE_R | PTE_W | PTE_U);
		mtx_unlock(&td->td_proc->p_lock);
		if (ret < 0)
			return ret;
		else
			return addr;
	}
}

/**
 * @brief 给予内核内存空间的访问建议
 * 暂不实现，返回0即可
 */
int sys_madvise(void *addr, size_t length, int advice) {
	warn("sys_madvise not implemented!\n");
	return 0;
}

int sys_membarrier(int cmd, int flags) {
	warn("sys_membarrier not implemented!\n");
	return 0;
}
