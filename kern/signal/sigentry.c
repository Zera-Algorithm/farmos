#include <lib/log.h>
#include <lib/transfer.h>
#include <mm/memlayout.h>
#include <proc/cpu.h>
#include <proc/sched.h>
#include <proc/thread.h>
#include <signal/machine.h>
#include <signal/signal.h>
#include <sys/syscall.h>

extern char trampoline[];

#define user_sig_return_uaddr SIGNAL_TRAMPOLINE

/**
 * @brief 获取当前最高优先级能处理的信号
 */
sigevent_t *sig_getse(thread_t *td) {
	assert(mtx_hold(&td->td_lock));
	sigevent_t *se = NULL;
	TAILQ_FOREACH (se, &td->td_sigqueue, se_link) {
		// 若信号正在处理或者信号没被阻塞，返回信号
		if ((se->se_status & SE_PROCESSING) || sig_td_canhandle(td, se->se_signo)) {
			return se;
		}
	}
	// 没有信号需要处理
	return NULL;
}

static void sig_beforestart(thread_t *td, sigevent_t *se, sigaction_t *sa) {
	assert(mtx_hold(&td->td_lock));
	// 设为处理状态
	se->se_status = SE_PROCESSING;
	td->td_sig = se;
	// 保存用户上下文及信号屏蔽字
	se->se_restoretf = td->td_trapframe;
	se->se_restoremask = td->td_cursigmask;
	// 更新当前的信号屏蔽字
	td->td_cursigmask = sigset_or(&td->td_cursigmask, &sa->sa_mask);
	// 保护用户栈（压栈）
	td->td_trapframe.sp -= 0x1000;
	// 设置返回地址为 sigreturn
	td->td_trapframe.ra = sa->sa_restorer ? (u64)sa->sa_restorer : user_sig_return_uaddr;
	// 使用户程序从 handler 启动
	td->td_trapframe.epc = (u64)sa->sa_handler;
	// 向a0填入信号参数
	td->td_trapframe.a0 = se->se_signo;
	// 若指定了 SA_SIGINFO，则补充参数
	if (sa->sa_flags & SA_SIGINFO) {
		siginfo_set(td, se);
		td->td_trapframe.sp -= sizeof(u64);
		td->td_trapframe.sp -= sizeof(u64);
	}
	// 留出参数空间
	td->td_trapframe.sp -= sizeof(u64);
}

static void sig_afterret(thread_t *td, sigevent_t *se, sigaction_t *sa) {
	assert(mtx_hold(&td->td_lock));
	// 恢复用户上下文及信号屏蔽字
	td->td_trapframe = se->se_restoretf;
	td->td_cursigmask = se->se_restoremask;
}

static bool sig_cancheck(thread_t *td) {
	if (td->td_killed) {
		warn("%s killed by signal\n", td->td_name);
		mtx_unlock(&td->td_lock);
		sys_exit(-1);
	}
	return true;
}

void sig_check() {
	// 获取当前线程
	thread_t *td = cpu_this()->cpu_running;
	// 信号处理逻辑
	mtx_lock(&td->td_lock);
	while (sig_cancheck(td)) {
		// 获取一个能够处理的信号
		sigevent_t *se = sig_getse(td);

		// 没有信号需要处理、或者信号正在处理时，退出循环
		if (se == NULL || se->se_status & SE_PROCESSING) {
			break;
		}

		// 有新的信号需要处理，先获取信号处理函数
		sigaction_t *sa = sigaction_get(td->td_proc, se->se_signo);

		// struct pthread self;
		// if (se->se_signo == 33) {
		// 	warn("sig_check: %s handling signal %d\n", td->td_name, se->se_signo);
		// 	copyIn(td->td_trapframe.tp - sizeof(struct pthread), &self,
		// 	       sizeof(struct pthread));
		// }

		// 判断信号处理函数是否存在（或者是默认（DFL=0）、忽略(IGN=1)）
		if (sa->sa_handler == NULL || sa->sa_handler == SIG_IGN) {
			// 未注册的信号处理函数
			// 检查默认处理函数
			if (se->se_signo == SIGKILL || se->se_signo == SIGTERM) {
				// 默认处理函数：终止进程
				warn("%s handling SIGKILL signal\n", td->td_name);
				td->td_killed = 1;
				continue;
			}
			warn("%s's signal %d ignored\n", td->td_name, se->se_signo);
			// 无默认处理函数，直接忽略
			sigeventq_remove(td, se);
			sigevent_free(se);
		} else {
			// 已注册的信号处理函数
			// 第一步：保存当前上下文
			warn("%s handling signal %d\n", td->td_name, se->se_signo);
			sig_beforestart(td, se, sa);
			// 跳出循环，返回用户态处理信号
			break;
		}
		// warn("%lx\n", self.tsd);
	}
	mtx_unlock(&td->td_lock);
}

void sig_return(thread_t *td) {
	sigevent_t *se = NULL;
	// 获取当前信号
	mtx_lock(&td->td_lock);
	TAILQ_FOREACH (se, &td->td_sigqueue, se_link) {
		if (se->se_status & SE_PROCESSING) {
			break;
		}
	}
	sig_afterret(td, se, sigaction_get(td->td_proc, se->se_signo));
	// 从信号队列中移除
	sigeventq_remove(td, se);
	// 释放信号
	sigevent_free(se);
	mtx_unlock(&td->td_lock);
}
