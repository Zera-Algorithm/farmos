#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <param.h>
#include <signal/sigset.h>
#include <types.h>

// 信号相关结构体
typedef struct siginfo {
	int si_signo;	      /* Signal number */
	int si_errno;	      /* An errno value */
	int si_code;	      /* Signal code */
	int si_trapno;	      /* Trap number that caused hardware-generated signal (unused on most
				 architectures) */
	pid_t si_pid;	      /* Sending process ID */
	uid_t si_uid;	      /* Real user ID of sending process */
	int si_status;	      /* Exit value or signal */
	clock_t si_utime;     /* User time consumed */
	clock_t si_stime;     /* System time consumed */
	sigval_t si_value;    /* Signal value */
	int si_int;	      /* POSIX.1b signal */
	void *si_ptr;	      /* POSIX.1b signal */
	int si_overrun;	      /* Timer overrun count; POSIX.1b timers */
	int si_timerid;	      /* Timer ID; POSIX.1b timers */
	void *si_addr;	      /* Memory location which caused fault */
	long si_band;	      /* Band event (was int in glibc 2.3.2 and earlier) */
	int si_fd;	      /* File descriptor */
	short si_addr_lsb;    /* Least significant bit of address (since Linux 2.6.32) */
	void *si_lower;	      /* Lower bound when address violation	occurred (since Linux 3.19) */
	void *si_upper;	      /* Upper bound when address violation	occurred ,(since Linux 3.19) */
	int si_pkey;	      /* Protection key on PTE that causedfault (since Linux 4.6) */
	void *si_call_addr;   /* Address of system call instruction	(since Linux 3.5) */
	int si_syscall;	      /* Number of attempted system call (since Linux 3.5) */
	unsigned int si_arch; /* Architecture of attempted system call (since Linux 3.5) */
} siginfo_t;

// typedef struct sigaction {
// 	void (*sa_handler)(int);
// 	void (*sa_sigaction)(int, siginfo_t *, void *);
// 	sigset_t sa_mask;
// 	int sa_flags;
// 	void (*sa_restorer)(void);
// } sigaction_t;

// 引用自musl的k_sigaction结构体
typedef struct k_sigaction {
	void (*sa_handler)(int);
	unsigned long sa_flags;
	void (*sa_restorer)(void);
	sigset_t sa_mask; // 可变长
} sigaction_t;

// 自定义字段
#include <lib/queue.h>
#include <trap/trapframe.h>

#define SE_PROCESSING 0x1

typedef struct sigevent {
	trapframe_t se_restoretf;
	sigset_t se_restoremask;
	int se_signo;
	int se_status;
	TAILQ_ENTRY(sigevent) se_link;
} sigevent_t;

typedef TAILQ_HEAD(, sigevent) sigeventq_t;
typedef struct proc proc_t;
typedef struct thread thread_t;

// 信号
void sig_init();

// 信号处理出入口
void sig_check();
void sig_return(thread_t *td);

// 信号事件相关函数
sigevent_t *sigevent_alloc(int signo) __attribute__((warn_unused_result));
void sigevent_free(sigevent_t *se);

// 信号处理相关函数
err_t sigaction_register(int signo, u64 act, u64 oldact, int sigset_size);
sigaction_t *sigaction_get(proc_t *p, int signo);

// 信号队列相关函数
void sigeventq_insert(thread_t *td, sigevent_t *se);
void sigeventq_remove(thread_t *td, sigevent_t *se);

// 信号相关宏
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGIOT SIGABRT
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPOLL SIGIO
#define SIGPWR 30
#define SIGSYS 31
#define SIGUNUSED SIGSYS

#endif // _SIGNAL_H
