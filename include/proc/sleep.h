#ifndef _SLEEP_H
#define _SLEEP_H

#include <types.h>

typedef struct mutex mutex_t;
typedef struct thread thread_t;

extern mutex_t wait_lock;

void sleep(void *chan, mutex_t *mtx, const char *msg);
void wakeup(void *chan);
void wakeup_td(thread_t *td);

u64 wait(thread_t *curtd, i64 pid, u64 pstatus, int options);

#endif
