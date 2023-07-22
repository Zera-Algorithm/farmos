#include <lib/log.h>
#include <lib/transfer.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <signal/signal.h>
#include <sys/syscall.h>

int sys_sigaction(int signum, u64 act, u64 oldact) {
	if (signum < 0 || signum >= SIGNAL_MAX) {
		return -1;
	} else {
		return sigaction_register(signum, act, oldact);
	}
}

int sys_sigreturn() {
	sig_return(cpu_this()->cpu_running);
	return 0;
}

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

int sys_sigprocmask(int how, u64 set, u64 oldset, size_t sigsetsize) {
	if (sigsetsize >= sizeof(sigset_t)) {
		return -1;
	}
	thread_t *td = cpu_this()->cpu_running;
	sigset_t newkset, oldkset;
	if (set) {
		copy_in(td->td_proc->p_pt, set, &newkset, sizeof(sigset_t));
	}
	mtx_lock(&td->td_lock);
	if (oldset) {
		oldkset = td->td_sigmask;
		copy_out(td->td_proc->p_pt, oldset, &oldkset, sizeof(sigset_t));
	}
	switch (how) {
	case SIG_BLOCK:
		if (set) {
			sigset_block(&td->td_sigmask, &newkset, sigsetsize);
		}
		break;
	case SIG_UNBLOCK:
		if (set) {
			sigset_unblock(&td->td_sigmask, &newkset, sigsetsize);
		}
		break;
	case SIG_SETMASK:
		td->td_sigmask = newkset;
		break;
	default:
		return -1;
	}
	mtx_unlock(&td->td_lock);
	return 0;
}

int sys_tkill(int tid, int sig) {
	if (sig < 0 || sig >= SIGNAL_MAX) {
		return -1;
	}

	// 这里在tid=0时，默认将tid设为当前线程
	if (tid == 0) {
		tid = cpu_this()->cpu_running->td_tid;
		warn("sys_tkill: tid == 0, tid = %d\n", tid);
	}

	thread_t *td = &threads[TID_TO_INDEX(tid)];

	if (tid != td->td_tid) {
		return -1;
	}
	mtx_lock(&td->td_lock);
	sigevent_t *se = sigevent_alloc(sig);
	sigeventq_insert(td, se);
	mtx_unlock(&td->td_lock);
	return 0;
}
