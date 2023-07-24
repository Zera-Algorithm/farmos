#include <lib/log.h>
#include <lib/string.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <trap/trap.h>

err_t page_fault_handler(u64 badva) {
	thread_t *td = cpu_this()->cpu_running;

	// 查找页表项
	pte_t pte = ptLookup(td->td_pt, badva);

	// 写时复制的条件：用户态、只读、写时复制
	if ((pte != 0) && (pte & PTE_U) && !(pte & PTE_W) && (pte & PTE_COW)) {
		u64 newpa = vmAlloc();
		u64 oldpa = pteToPa(pte);
		memcpy((void *)newpa, (void *)oldpa, PAGE_SIZE);
		u64 newperm = (PTE_PERM(pte) & ~PTE_COW) | PTE_W;
		return ptMap(td->td_pt, badva, newpa, newperm);
	} else {
		// 不合法的写入请求 todo: signal
		char prot[] = "urwxc";
		if (pte & PTE_U)
			prot[0] = 'U';
		if (pte & PTE_R)
			prot[1] = 'R';
		if (pte & PTE_W)
			prot[2] = 'W';
		if (pte & PTE_X)
			prot[3] = 'X';
		if (pte & PTE_COW)
			prot[4] = 'C';

		warn("page fault: badva=%lx, pte=%lx (prot = %s)\n", badva, pte, prot);
		return -1; // todo errcode
	}
}
