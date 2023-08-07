#include <signal/itimer.h>
#include <proc/proc.h>
#include <dev/timer.h>
#include <proc/thread.h>
#include <signal/signal.h>
#include <lib/log.h>
#include <lib/string.h>

itimer_list_t itimer_list;

#define getTime time_rtc_clock

void itimer_init() {
	LIST_INIT(&itimer_list.itimer_head);
	mtx_init(&itimer_list.itimer_lock, "itimer_list", false, MTX_SPIN | MTX_RECURSE);
}

static void cycle_to_timeval(u64 cycles, struct timeval *val) {
	val->tv_usec = (cycles / CLOCK_PER_USEC) % USEC_PER_SEC;
	val->tv_sec = cycles / CLOCK_PER_SEC;
}

static u64 timeval_to_cycle(struct timeval *val) {
	return val->tv_sec * CLOCK_PER_SEC + val->tv_usec * CLOCK_PER_USEC;
}

/**
 * @brief 搜索itimer列表，返回指定线程的itimer。如果不存在，返回全0的itimerval
 */
void itimer_get(thread_t *td, struct itimerval *itv) {
	mtx_lock(&itimer_list.itimer_lock);

	itimer_info_t *itimer;
	LIST_FOREACH(itimer, &itimer_list.itimer_head, link) {
		if (itimer->td == td) {
			cycle_to_timeval(itimer->interval, &itv->it_interval);
			cycle_to_timeval(itimer->start + itimer->last_time - getTime(), &itv->it_value);
			mtx_unlock(&itimer_list.itimer_lock);
			return;
		}
	}

	// 不存在itimer
	cycle_to_timeval(0, &itv->it_interval);
	cycle_to_timeval(0, &itv->it_value);
	mtx_unlock(&itimer_list.itimer_lock);
}

static inline void itimer_remove(itimer_info_t *itimer) {
	LIST_REMOVE(itimer, link);
	kfree(itimer);
}

static inline itimer_info_t * itimer_alloc() {
	itimer_info_t * itimer = kmalloc(sizeof(itimer_info_t));
	LIST_INSERT_HEAD(&itimer_list.itimer_head, itimer, link);
	return itimer;
}

/**
 * @brief 找到指定线程的itimer，并更新其值（没有就新建）。如果it_value为0，表示禁用计时器
 */
void itimer_update(thread_t *td, struct itimerval *itv) {
	mtx_lock(&itimer_list.itimer_lock);
	u64 start = getTime();
	u64 last_time = timeval_to_cycle(&itv->it_value);
	u64 interval = timeval_to_cycle(&itv->it_interval);

	itimer_info_t *itimer;
	LIST_FOREACH(itimer, &itimer_list.itimer_head, link) {
		if (itimer->td == td) {
			if (last_time == 0) {
				itimer_remove(itimer);
			} else {
				// 重设下一次到期值（需要同时设定start和last_time
				itimer->start = start;
				itimer->last_time = last_time;
			}
			itimer->interval = interval;
			mtx_unlock(&itimer_list.itimer_lock);
			return;
		}
	}

	// 未找到，仅当it_value不为0时考虑新建
	if (last_time != 0) {
		itimer = itimer_alloc();
		itimer->td = td;
		itimer->start = start;
		itimer->last_time = last_time;
		itimer->interval = interval;
	}
	mtx_unlock(&itimer_list.itimer_lock);
}

/**
 * @brief 取消一个线程的计时器
 */
void itimer_cancel(thread_t *td) {
	struct itimerval itimerval;
	memset(&itimerval, 0, sizeof(itimerval));
	itimer_update(td, &itimerval);
}

/**
 * @brief 时钟中断时检查是否有可以发信号的线程，找到时间到期的项目。
 * 如果是周期性，则发信号并更新条目，否则删除原条目。
 */
void itimer_check() {
	mtx_lock(&itimer_list.itimer_lock);
	u64 now = getTime();

	itimer_info_t *itimer, *tmp;
	LIST_FOREACH_PARTIAL_DEL(itimer, &itimer_list.itimer_head) {
		if (itimer->start + itimer->last_time <= now) {
			// 到期
			warn("itimer of thread %s is expired, send signal\n", itimer->td->td_name);
			sig_send_td(itimer->td, SIGALRM);
			if (itimer->interval != 0) {
				// 周期性，重新设置到期时间
				itimer->last_time = itimer->interval;
				itimer->start = now;
				itimer = LIST_NEXT(itimer, link);
			} else {
				// 要删除时，需要预先保存当前项的下一项
				tmp = LIST_NEXT(itimer, link);
				itimer_remove(itimer);
				itimer = tmp;
			}
		} else {
			itimer = LIST_NEXT(itimer, link);
		}
	}

	mtx_unlock(&itimer_list.itimer_lock);
}
