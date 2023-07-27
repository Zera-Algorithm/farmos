#ifndef _SIGSET_H
#define _SIGSET_H

#include <lib/error.h>
#include <types.h>

// 信号处理状态
#define SIGNAL_MAX 128

typedef struct sigset {
	u8 ss_byte[(SIGNAL_MAX + 7) / 8];
} sigset_t;

// signo从1开始

static inline void sigset_init(sigset_t *set) {
	for (int i = 0; i < sizeof(set->ss_byte); i++) {
		set->ss_byte[i] = 0;
	}
}

static inline void sigset_set(sigset_t *set, int signo) {
	signo -= 1;
	assert(0 <= signo && signo < SIGNAL_MAX);
	set->ss_byte[signo / 8] |= 1 << (signo % 8);
}

static inline void sigset_clear(sigset_t *set, int signo) {
	signo -= 1;
	assert(0 <= signo && signo < SIGNAL_MAX);
	set->ss_byte[signo / 8] &= ~(1 << (signo % 8));
}

static inline bool sigset_isset(sigset_t *set, int signo) {
	signo -= 1;
	assert(0 <= signo && signo < SIGNAL_MAX);
	return set->ss_byte[signo / 8] & (1 << (signo % 8));
}

static inline sigset_t sigset_or(sigset_t *set1, sigset_t *set2) {
	sigset_t set;
	for (int i = 0; i < sizeof(set.ss_byte); i++) {
		set.ss_byte[i] = set1->ss_byte[i] | set2->ss_byte[i];
	}
	return set;
}

static inline void sigset_block(sigset_t *dst, sigset_t *src, u64 size) {
	int limit = MIN(sizeof(dst->ss_byte), size);
	for (int i = 0; i < limit; i++) {
		dst->ss_byte[i] |= src->ss_byte[i];
	}
}

static inline void sigset_unblock(sigset_t *dst, sigset_t *src, u64 size) {
	int limit = MIN(sizeof(dst->ss_byte), size);
	for (int i = 0; i < limit; i++) {
		dst->ss_byte[i] &= ~src->ss_byte[i];
	}
}

#endif // _SIGSET_H
