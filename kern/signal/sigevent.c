#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <signal/signal.h>

typedef sigaction_t sigprocactions_t[SIGNAL_MAX];
sigprocactions_t *sigactions;

sigevent_t *sigevents;

sigeventq_t sigevent_freeq;
mutex_t sigevent_lock;

void sig_init() {
	// 初始化信号事件
	mtx_init(&sigevent_lock, "sigevent lock", false, MTX_SPIN | MTX_RECURSE);
	for (int i = NSIGEVENTS - 1; i >= 0; i--) {
		TAILQ_INSERT_HEAD(&sigevent_freeq, &sigevents[i], se_link);
	}
	// 初始化信号动作
	memset(sigactions, 0, sizeof(sigaction_t) * SIGNAL_MAX * NPROC);
}

// 信号事件分配

sigevent_t *sigevent_alloc(int signo) {
	mtx_lock(&sigevent_lock);
	sigevent_t *se = TAILQ_FIRST(&sigevent_freeq);
	assert(se != NULL);
	TAILQ_REMOVE(&sigevent_freeq, se, se_link);
	mtx_unlock(&sigevent_lock);
	se->se_signo = signo;
	return se;
}

void sigevent_free(sigevent_t *se) {
	mtx_lock(&sigevent_lock);
	// 确保remove掉，然后memset
	memset(se, 0, sizeof(sigevent_t));
	TAILQ_INSERT_HEAD(&sigevent_freeq, se, se_link);
	mtx_unlock(&sigevent_lock);
}

void sigevent_freetd(thread_t *td) {
	mtx_lock(&sigevent_lock);
	sigevent_t *se;
	while ((se = TAILQ_FIRST(&td->td_sigqueue)) != NULL) {
		sigeventq_remove(td, se);
		sigevent_free(se);
	}
	mtx_unlock(&sigevent_lock);
}

// 信号处理函数注册

err_t sigaction_register(int signo, u64 act, u64 oldact, int sigset_size) {
	assert(0 < signo && signo <= SIGNAL_MAX);
	thread_t *td = cpu_this()->cpu_running;
	sigaction_t *kact = &sigactions[td->td_proc - procs][signo - 1];

	// ksigaction实际的大小：考虑到sigset_t的可变长
	size_t ksa_size = sizeof(sigaction_t) - sizeof(sigset_t) + sigset_size;

	if (oldact != 0) {
		copy_out(td->td_proc->p_pt, oldact, kact, ksa_size);
	}
	if (act != 0) {
		memset(kact, 0, sizeof(sigaction_t));
		copy_in(td->td_proc->p_pt, act, kact, ksa_size);
		if (!(kact->sa_flags & SA_RESTORER)) {
			kact->sa_restorer = 0;
		}
		if (0x1 < (u64)kact->sa_handler && (u64)kact->sa_handler < 0x10000ul) {
			error("sigaction_register: invalid handler %p", kact->sa_handler);
		}
	}
	return 0;
}

void sigaction_free(proc_t *p) {
	memset(&sigactions[p - procs], 0, sizeof(sigactions[0]));
}

void sigaction_clone(proc_t *p, proc_t *childp) {
	memcpy(&sigactions[childp - procs], &sigactions[p - procs], sizeof(sigactions[0]));
}

/**
 * @brief 返回对应的信号处理动作
 */
sigaction_t *sigaction_get(proc_t *p, int signo) {
	assert(0 < signo && signo <= SIGNAL_MAX);
	return &sigactions[p - procs][signo - 1];
}
