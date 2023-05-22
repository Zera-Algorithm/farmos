#include <proc/proc.h>
#include <syscall.h>

u64 sysBrk(u64 addr);
u64 sysMunmap(u64 start, u64 len);
u64 sysMmap(u64 addr, u64 len, u64 prot, u64 flags, u64 fd, u64 offset);

void syscallEntry(Trapframe *tf) {
	u64 sysno = tf->a7;
	u64 ret = tf->a0;
	// 根据系统调用号，调用对应的系统调用处理函数
	warn("unimplement: syscall %d\n", sysno);

	// 根据反汇编结果，一个ecall占用4字节的空间
	tf->epc += 4;
	return;
	switch (sysno) {
	case SYS_brk:
		ret = sysBrk(tf->a0);
		break;
	case SYS_munmap:
		ret = sysMunmap(tf->a0, tf->a1);
		break;
	case SYS_mmap:
		ret = sysMmap(tf->a0, tf->a1, tf->a2, tf->a3, tf->a4, tf->a5);
		break;
	default:
		panic("unknown syscall %d.\n", sysno);
	}
	// 将系统调用返回值放入a0寄存器
	tf->a0 = ret;
}

u64 sysMunmap(u64 start, u64 len) {
	u64 from = PGROUNDUP(start);
	u64 to = PGROUNDDOWN(start + len);
	for (u64 va = from; va < to; va += PAGE_SIZE) {
		// 释放虚拟地址所在的页
		catchMemErr(pageRemove(myProc()->pageTable, va));
	}
	return 0;
}
