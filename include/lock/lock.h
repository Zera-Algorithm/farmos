#ifndef _LOCK_H_
#define _LOCK_H_

#include <types.h>

typedef struct lock_object {
	const char *lo_name;
	u64 lo_locked;
	void *lo_data;
} lock_object_t;

#endif // _LOCK_H_
