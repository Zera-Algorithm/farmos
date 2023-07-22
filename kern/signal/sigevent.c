#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <signal/signal.h>

#define NSIG 512

sigaction_t sigactions[NPROC][SIGNAL_MAX];

sigevent_t sigevents[NSIG];
sigeventq_t sigevent_freeq;
mutex_t sigevent_lock;

void sig_init() {
	// 初始化信号事件
	mtx_init(&sigevent_lock, "sigevent lock", false, MTX_SPIN);
	for (int i = NSIG - 1; i >= 0; i--) {
		TAILQ_INSERT_HEAD(&sigevent_freeq, &sigevents[i], se_link);
	}
	// 初始化信号动作
	memset(sigactions, 0, sizeof(sigactions));
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
	TAILQ_INSERT_HEAD(&sigevent_freeq, se, se_link);
	mtx_unlock(&sigevent_lock);
}

// 信号处理函数注册

err_t sigaction_register(int signo, u64 act, u64 oldact) {
	assert(0 <= signo && signo < SIGNAL_MAX);
	thread_t *td = cpu_this()->cpu_running;
	sigaction_t *kact = &sigactions[td->td_proc - procs][signo];
	if (oldact != 0) {
		copy_out(td->td_proc->p_pt, oldact, kact, sizeof(sigaction_t));
	}
	if (act != 0) {
		copy_in(td->td_proc->p_pt, act, kact, sizeof(sigaction_t));
	}
	return 0;
}

/**
 * @brief 返回对应的信号处理动作
 */
sigaction_t *sigaction_get(proc_t *p, int signo) {
	assert(0 <= signo && signo < SIGNAL_MAX);
	return &sigactions[p - procs][signo];
}
