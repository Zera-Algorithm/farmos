#include <lib/log.h>
#include <lib/string.h>
#include <mm/vmm.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <riscv.h>
#include <trap/trap.h>
#include <sys/syscall.h>
#include <lib/printf.h>
#include <lib/terminal.h>

err_t cow_handler(pte_t *pd, pte_t pte, u64 badva) {
	u64 newpa = vmAlloc();
	u64 oldpa = pteToPa(pte);
	memcpy((void *)newpa, (void *)oldpa, PAGE_SIZE);
	u64 newperm = (PTE_PERM(pte) & ~PTE_COW) | PTE_W;
	return ptMap(pd, badva, newpa, newperm);
}

err_t passive_handler(pte_t *pd, pte_t pte, u64 badva) {
	assert((pte & PTE_PPNMASK) == 0);
	u64 newpa = vmAlloc();
	u64 perm = pte ^ PTE_PASSIVE;
	warn("passive page fault: badva=%lx, pte=%lx, perm=%lx\n", badva, pte, perm);
	return ptMap(pd, badva, newpa, perm);
}

err_t page_fault_handler(pte_t *pd, u64 violate, u64 badva) {
	// 查找页表项
	pte_t pte = ptLookup(pd, badva);

	if ((violate & PTE_W) && (pte != 0) && (pte & PTE_U) && !(pte & PTE_W) && (pte & PTE_COW)) {
		// 写时复制：写错误且用户位、只读位、写时复制位
		return cow_handler(pd, pte, badva);
	} else if (pte & PTE_PASSIVE) {
		// 被动调页：不管违反了哪种权限，如果那一页是被动映射的，就先映射上
		return passive_handler(pd, pte, badva);
	} else {
		// 不合法的写入请求 todo: signal
		return -1; // todo errcode
	}
}


void trap_pgfault(thread_t *td, u64 exc_code) {
	u64 badva = r_stval() & ~(PAGE_SIZE - 1);
	u64 violation;
	switch (exc_code) {
		case EXCCODE_STORE_PAGE_FAULT:
			violation = PTE_W;
			break;
		case EXCCODE_LOAD_PAGE_FAULT:
			violation = PTE_R;
			break;
		default:
			panic("trap_pgfault: unsupported exc_code");
	}

	if (page_fault_handler(td->td_proc->p_pt, violation, badva)) {
		// 页错误处理失败，发送 SIGSEGV 信号
		pte_t pte = ptLookup(td->td_pt, badva);
		char prot[] = "     ";
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

		printf(FARM_ERROR"[Page Fault] "SGR_RED
			"%s(t:%08x|p:%08x) violate %x, badva=%lx, pte=%lx (prot = %s)"SGR_RESET"\n",
			td->td_name, td->td_tid, td->td_proc->p_pid, violation, badva, pte, prot);
		warn("page fault caused 'SIGSEGV' on thread %d[%s]\n", td->td_tid, td->td_name);
		sig_send_proc(td->td_proc, SIGSEGV);
		sys_exit(-1);
	}
}
