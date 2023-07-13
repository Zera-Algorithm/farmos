#include <lib/log.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/syscall.h>

err_t sys_unmap(u64 start, u64 len) {
	u64 from = PGROUNDUP(start);
	u64 to = PGROUNDDOWN(start + len - 1);
	for (u64 va = from; va <= to; va += PAGE_SIZE) {
		// 释放虚拟地址所在的页
		panic_on(ptUnmap(cpu_this()->cpu_running->td_pt, va));
	}
	return 0;
}

err_t sys_map(u64 start, u64 len, u64 perm) {
	u64 from = PGROUNDDOWN(start);
	u64 to = PGROUNDUP(start + len);
	pte_t *pt = cpu_this()->cpu_running->td_pt;
	for (u64 va = from; va <= to; va += PAGE_SIZE) {
		// 若虚拟地址对应的物理地址不存在，则分配一个物理页
		if (pteToPa(ptLookup(pt, va)) == 0) {
			u64 pa = vmAlloc();
			panic_on(ptMap(pt, va, pa, perm));
		}
	}
	return 0;
}

err_t sys_brk(u64 addr) {
	thread_t *td = cpu_this()->cpu_running;
	u64 cur_brk = td->td_brk;
	if (addr == 0) {
		return cur_brk;
	} else if (addr < cur_brk) {
		// 缩短堆
		td->td_brk = addr;
		return sys_unmap(addr + 1, cur_brk);
	} else {
		// 伸长堆
		td->td_brk = addr;
		return sys_map(cur_brk + 1, addr - cur_brk, PTE_R | PTE_W | PTE_U);
	}
}
