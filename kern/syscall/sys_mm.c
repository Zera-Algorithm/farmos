#include <lib/log.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/interface.h>
#include <proc/proc.h>
#include <proc/thread.h>
#include <sys/syscall.h>

err_t sys_map(u64 start, u64 len, u64 perm) {
	u64 from = PGROUNDDOWN(start);
	u64 to = PGROUNDUP(start + len);
	pte_t *pt = cur_proc_pt();
	for (u64 va = from; va <= to; va += PAGE_SIZE) {
		// 若虚拟地址对应的物理地址不存在，则分配一个物理页
		if (pteToPa(ptLookup(pt, va)) == 0) {
			u64 pa = vmAlloc();
			// 如果没有多余内存可供分配，则返回错误
			if (pa == 0) {
				warn("sys_map: vmAlloc failed!\n");
				return -1;
			}
			panic_on(ptMap(pt, va, pa, perm));
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
		ret = sys_map(cur_brk + 1, addr - cur_brk, PTE_R | PTE_W | PTE_U);
		mtx_unlock(&td->td_proc->p_lock);
		if (ret < 0)
			return ret;
		else
			return addr;
	}
}
