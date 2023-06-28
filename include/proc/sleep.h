#ifndef _SLEEP_H
#define _SLEEP_H

#include <proc/thread.h>
#include <types.h>

void sleep(void *chan, mutex_t *mtx, const char *msg);
void wakeup(void *chan);

#endif
