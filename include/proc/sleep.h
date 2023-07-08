#ifndef _SLEEP_H
#define _SLEEP_H

#include <types.h>

typedef struct mutex mutex_t;

void sleep(void *chan, mutex_t *mtx, const char *msg);
void wakeup(void *chan);

#endif
