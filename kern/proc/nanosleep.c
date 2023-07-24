#include <dev/timer.h>
#include <lib/error.h>
#include <lock/mutex.h>
#include <proc/nanosleep.h>
#include <proc/cpu.h>
#include <proc/sleep.h>
#include <proc/thread.h>

static nanosleep_t nanosleep_struct[NPROC];
mutex_t mtx_nanosleep;
struct nanosleep_list nanosleep_list = {(nanosleep_t *)NULL};

static nanosleep_t *nanosleep_alloc() {
	for (int i = 0; i < NPROC; i++) {
		if (nanosleep_struct[i].valid == 0) {
			nanosleep_struct[i].valid = 1;
			return &nanosleep_struct[i];
		}
	}
	panic("no more nanosleep struct to alloc");
	return NULL;
}

static void nanosleep_dealloc(nanosleep_t *nanosleep) {
	nanosleep->valid = 0;
	LIST_REMOVE(nanosleep, nano_link);
}

/**
 * @brief 对当前线程执行线程睡眠
 */
void nanosleep_proc(u64 clocks) {
	mtx_lock(&mtx_nanosleep);
	nanosleep_t *nanosleep = nanosleep_alloc();

	nanosleep->clocks = clocks;
	nanosleep->start_time = getTime();
	nanosleep->end_time = nanosleep->start_time + clocks;
	nanosleep->td = cpu_this()->cpu_running;

	// 插入队列，保证队列按end_time升序排列
	if (LIST_EMPTY(&nanosleep_list)) {
		LIST_INSERT_HEAD(&nanosleep_list, nanosleep, nano_link);
	} else {
		int insert = 0;
		nanosleep_t *tmp, *last = NULL;
		LIST_FOREACH (tmp, &nanosleep_list, nano_link) {
			last = tmp;
			if (tmp->end_time > nanosleep->end_time) {
				LIST_INSERT_BEFORE(tmp, nanosleep, nano_link);
				insert = 1;
				break;
			}
		}

		// 插入到最后
		if (!insert) {
			LIST_INSERT_AFTER(last, nanosleep, nano_link);
		}
	}

	sleep(nanosleep, &mtx_nanosleep, "nanosleep");
	mtx_unlock(&mtx_nanosleep);
}

/**
 * @brief 在每次时钟中断时检查是否能唤醒程序。对于不能唤醒的情况，检测速度很快
 */
void nanosleep_check() {
	mtx_lock(&mtx_nanosleep);
	u64 curTime = getTime();

	while (1) {
		if (LIST_EMPTY(&nanosleep_list) ||
		    LIST_FIRST(&nanosleep_list)->end_time > curTime) {
			break;
		}
		nanosleep_t *first = LIST_FIRST(&nanosleep_list);
		wakeup(first);
		nanosleep_dealloc(first);
	}

	mtx_unlock(&mtx_nanosleep);
}

/**
 * @brief 提前唤醒处于nanosleep状态的线程
 * 不检验线程是否处于nanosleep状态，可能在调用之前就已因时间到期而唤醒，调用者需要自行保证正确性
 * 如果线程在nanosleep队列中，提前唤醒不影响原有的到期唤醒功能
 */
void nanosleep_early_wakeup(thread_t *td) {
	mtx_lock(&mtx_nanosleep);
	nanosleep_t *tmp;
	LIST_FOREACH (tmp, &nanosleep_list, nano_link) {
		if (tmp->td == td) {
			wakeup(tmp);
			nanosleep_dealloc(tmp);
			break;
		}
	}
	mtx_unlock(&mtx_nanosleep);
}
