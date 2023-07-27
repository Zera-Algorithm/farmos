#include <lib/string.h>
#include <lib/transfer.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <signal/signal.h>

void siginfo_set(thread_t *td, sigevent_t *se) {
	// 从用户栈中分配 siginfo_t 结构体
	td->td_trapframe.sp = (td->td_trapframe.sp - sizeof(siginfo_t)) & (~0xf);
	// 将 siginfo_t 结构体的地址填入 a1
	td->td_trapframe.a1 = td->td_trapframe.sp;
	// 从用户栈中分配 ucontext_t 结构体
	td->td_trapframe.sp = (td->td_trapframe.sp - sizeof(ucontext_t)) & (~0xf);
	// 将 ucontext_t 结构体的地址填入 a2
	td->td_trapframe.a2 = td->td_trapframe.sp;

	// 初始化 siginfo 结构体
	siginfo_t siginfo = {0};
	siginfo.si_signo = se->se_signo;

	// 初始化 ucontext 结构体
	ucontext_t uctx = {0};
	memcpy(&uctx.uc_sigmask, &td->td_sigmask, sizeof(sigset_t));
	uctx.uc_mcontext.MC_PC = se->se_restoretf.epc;

	// 拷贝到用户空间
	se->se_usiginfo = td->td_trapframe.a1;
	se->se_uuctx = td->td_trapframe.a2;
	// 向用户空间拷贝 siginfo_t 结构体
	copy_out(td->td_proc->p_pt, se->se_usiginfo, &siginfo, sizeof(siginfo_t));
	// 向用户空间拷贝 ucontext_t 结构体
	copy_out(td->td_proc->p_pt, se->se_uuctx, &uctx, sizeof(ucontext_t));
}

void siginfo_return(thread_t *td, sigevent_t *se) {
	ucontext_t uctx_ret;
	// 从用户空间拷贝 ucontext_t 结构体
	copy_in(td->td_proc->p_pt, se->se_uuctx, &uctx_ret, sizeof(ucontext_t));
	// 将给定的 ucontext_t 结构体还原
	if (uctx_ret.uc_mcontext.MC_PC != 0) {
		se->se_restoretf.epc = uctx_ret.uc_mcontext.MC_PC;
	}
	memcpy(&td->td_sigmask, &uctx_ret.uc_sigmask, sizeof(sigset_t));
}
