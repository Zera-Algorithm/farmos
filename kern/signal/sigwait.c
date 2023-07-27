#include <dev/timer.h>
#include <proc/thread.h>
#include <proc/tsleep.h>
#include <signal/signal.h>

void sig_timedwait(thread_t *td, sigset_t *set, siginfo_t *info, u64 timeout) {
	// todo check set

	if (timeout != 0) {
		tsleep(td, &td->td_lock, "#sigwait", timeout + getUSecs());
	}

	sigevent_t *se = sig_getse(td);
	if (se != NULL) {
		info->si_signo = se->se_signo;
	}
}