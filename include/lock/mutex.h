#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <lock/lock.h>
#include <types.h>

typedef struct thread thread_t;

typedef struct mutex {
	lock_object_t mtx_lock_object;
	thread_t *mtx_owner; // 仅在睡眠锁中使用
	bool mtx_debug;	     // 是否输出调试信息
	u8 mtx_type;	     // 锁的类型
	u8 mtx_depth;	     // 锁的深度（意义与类型相关）
} mutex_t;

#define MTX_SPIN 0x01
#define MTX_SLEEP 0x02
#define MTX_RECURSE 0x04

void mtx_init(mutex_t *m, const char *name, bool debug, u8 type);
void mtx_set(mutex_t *m, const char *name, bool debug);

void mtx_lock(mutex_t *mtx);
bool mtx_try_lock(mutex_t *mtx);
void mtx_unlock(mutex_t *mtx);

void mtx_lock_sleep(mutex_t *m);
void mtx_unlock_sleep(mutex_t *m);

bool mtx_hold(mutex_t *mtx);

#endif // _MUTEX_H_
