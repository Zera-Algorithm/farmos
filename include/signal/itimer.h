#ifndef _ITIMER_H
#define _ITIMER_H
#include <types.h>
#include <sys/time.h>
#include <lib/queue.h>
#include <lock/mutex.h>
#include <mm/kmalloc.h>

struct itimerval {
	struct timeval it_interval; /* Interval for periodic timer */
	struct timeval it_value;    /* Time until next expiration */
};

typedef struct itimer_info {
	u64 start; // 定时器的开始时间(clock)
	u64 last_time; // 此定时器的持续时间
	u64 interval; // 定时器的间隔时间(clock)，若为0表示非周期定时器
	thread_t *td; // 定时器所属的线程
	LIST_ENTRY(itimer_info) link; // 定时器链表
} itimer_info_t;

typedef struct itimer_list {
    LIST_HEAD(, itimer_info) itimer_head;
    mutex_t itimer_lock;
} itimer_list_t;

void itimer_init();
void itimer_get(thread_t *td, struct itimerval *itv);
void itimer_update(thread_t *td, struct itimerval *itv);
void itimer_cancel(thread_t *td);
void itimer_check();



#endif