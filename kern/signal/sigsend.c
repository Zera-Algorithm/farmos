#include <lib/log.h>
#include <lib/transfer.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <signal/signal.h>

bool sig_td_canhandle(thread_t *td, int signo) {
	assert(td != NULL);
	mtx_lock(&td->td_lock);
	sigset_t sigset = sigset_or(&td->td_sigmask, &td->td_cursigmask);
	bool canhandle = !sigset_isset(&sigset, signo);
	mtx_unlock(&td->td_lock);
	return canhandle;
}

void sig_send_td(thread_t *td, int signo) {
	warn("%lx send signal %d to thread %lx\n", cpu_this()->cpu_running->td_tid, signo,
	     td->td_tid);

	assert(td != NULL);
	mtx_lock(&td->td_lock);
	sigevent_t *se = sigevent_alloc(signo);
	sigeventq_insert(td, se);
	mtx_unlock(&td->td_lock);
}

void sig_send_proc(proc_t *p, int signo) {
	assert(p != NULL);
	proc_lock(p);

	// 先争取找到可以立马处理的线程
	thread_t *td;
	TAILQ_FOREACH (td, &p->p_threads, td_plist) {
		mtx_lock(&td->td_lock);
		// 从头部开始找到第一个可以处理该信号的线程
		if (sig_td_canhandle(td, signo)) {
			sig_send_td(td, signo);
			mtx_unlock(&td->td_lock);
			proc_unlock(p);
			return;
		}
		mtx_unlock(&td->td_lock);
	}

	// 如果没有找到可以处理该信号的线程，就发送给主线程
	td = TAILQ_FIRST(&p->p_threads);
	mtx_lock(&td->td_lock);
	sig_send_td(td, signo);
	mtx_unlock(&td->td_lock);
	proc_unlock(p);
}
