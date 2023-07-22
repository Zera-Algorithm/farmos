#include <lib/log.h>
#include <lock/mutex.h>
#include <proc/thread.h>
#include <signal/signal.h>

void sigeventq_insert(thread_t *td, sigevent_t *se) {
	assert(mtx_hold(&td->td_lock));
	TAILQ_INSERT_HEAD(&td->td_sigqueue, se, se_link);
}

void sigeventq_remove(thread_t *td, sigevent_t *se) {
	assert(mtx_hold(&td->td_lock));
	TAILQ_REMOVE(&td->td_sigqueue, se, se_link);
}