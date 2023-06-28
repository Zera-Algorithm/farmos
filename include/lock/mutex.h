#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <lock/lock.h>
#include <types.h>

typedef struct mutex {
	lock_object_t mtx_lock_object;
	void *mtx_owner;
	bool mtx_debug;
} mutex_t;

void mtx_init(mutex_t *m, const char *name, bool debug);
void mtx_set(mutex_t *m, const char *name, bool debug);

void mtx_lock(mutex_t *mtx);
void mtx_unlock(mutex_t *mtx);
bool mtx_hold(mutex_t *mtx);

#endif // _MUTEX_H_