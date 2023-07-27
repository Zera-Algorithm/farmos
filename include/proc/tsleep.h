#ifndef _TSLEEP_H
#define _TSLEEP_H

#include <lock/mutex.h>
#include <types.h>

void tsleep_init();
err_t tsleep(void *chan, mutex_t *mtx, const char *msg, u64 wakeus);
void twakeup(void *chan);
void tsleep_check();

#endif // _TSLEEP_H