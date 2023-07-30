#ifndef _USIGNAL_H
#define _USIGNAL_H

// 信号处理状态
#define SIGNAL_MAX 128

typedef struct sigset {
	unsigned char ss_byte[(SIGNAL_MAX + 7) / 8];
} sigset_t;

static inline void sigset_init(sigset_t *set) {
	for (int i = 0; i < sizeof(set->ss_byte); i++) {
		set->ss_byte[i] = 0;
	}
}

static inline void sigset_set(sigset_t *set, int signo) {
	set->ss_byte[signo / 8] |= 1 << (signo % 8);
}

static inline void sigset_clear(sigset_t *set, int signo) {
	set->ss_byte[signo / 8] &= ~(1 << (signo % 8));
}

static inline int sigset_isset(sigset_t *set, int signo) {
	return (set->ss_byte[signo / 8] & (1 << (signo % 8))) != 0;
}

static inline sigset_t sigset_or(sigset_t *set1, sigset_t *set2) {
	sigset_t set;
	for (int i = 0; i < sizeof(set.ss_byte); i++) {
		set.ss_byte[i] = set1->ss_byte[i] | set2->ss_byte[i];
	}
	return set;
}

typedef struct siginfo {
	int si_signo;		/* Signal number */
	int si_errno;		/* An errno value */
	int si_code;		/* Signal code */
	int si_trapno;		/* Trap number that caused hardware-generated signal (unused on most
				   architectures) */
	int si_pid;		/* Sending process ID */
	unsigned int si_uid;	/* Real user ID of sending process */
	int si_status;		/* Exit value or signal */
	long si_utime;		/* User time consumed */
	long si_stime;		/* System time consumed */
	unsigned long si_value; /* Signal value */
	int si_int;		/* POSIX.1b signal */
	void *si_ptr;		/* POSIX.1b signal */
	int si_overrun;		/* Timer overrun count; POSIX.1b timers */
	int si_timerid;		/* Timer ID; POSIX.1b timers */
	void *si_addr;		/* Memory location which caused fault */
	long si_band;		/* Band event (was int in glibc 2.3.2 and earlier) */
	int si_fd;		/* File descriptor */
	short si_addr_lsb;	/* Least significant bit of address (since Linux 2.6.32) */
	void *si_lower;		/* Lower bound when address violation	occurred (since Linux 3.19) */
	void *si_upper;	      /* Upper bound when address violation	occurred ,(since Linux 3.19) */
	int si_pkey;	      /* Protection key on PTE that causedfault (since Linux 4.6) */
	void *si_call_addr;   /* Address of system call instruction	(since Linux 3.5) */
	int si_syscall;	      /* Number of attempted system call (since Linux 3.5) */
	unsigned int si_arch; /* Architecture of attempted system call (since Linux 3.5) */
} siginfo_t;

typedef struct sigaction {
	void (*sa_handler)(int);
	unsigned long sa_flags;
	void (*sa_restorer)(void);
	sigset_t sa_mask; // 可变长
} sigaction_t;

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

#endif
