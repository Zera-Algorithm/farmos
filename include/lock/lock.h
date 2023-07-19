#ifndef _LOCK_H_
#define _LOCK_H_

#include <types.h>

typedef struct lock_object {
	const char *lo_name;
	u64 lo_locked;
	void *lo_data;
} lock_object_t;

// 中断使能栈操作
void lo_critical_enter();
void lo_critical_leave();

// 原子操作接口
void lo_acquire(lock_object_t *lo);
bool lo_try_acquire(lock_object_t *lo);
void lo_release(lock_object_t *lo);
bool lo_acquired(lock_object_t *lo) __attribute__((warn_unused_result));

#endif // _LOCK_H_
