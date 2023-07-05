#include <lib/log.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <riscv.h>

#define atomic_lock(ptr) __sync_lock_test_and_set(ptr, 1)
#define atomic_unlock(ptr) __sync_lock_release(ptr)
#define atomic_barrier() __sync_synchronize()

#define mtx_spin_debug(...)                                                                        \
	do {                                                                                       \
		if (m->mtx_debug) {                                                                \
			log(MUTEX_SPIN, __VA_ARGS__);                                              \
		}                                                                                  \
	} while (0)

#define mtx_sleep_debug(...)                                                                       \
	do {                                                                                       \
		if (m->mtx_debug) {                                                                \
			log(MUTEX_SLEEP, __VA_ARGS__);                                             \
		}                                                                                  \
	} while (0)

// 中断使能栈操作
static void mtx_enter() {
	register_t before = intr_disable();
	cpu_t *cpu = cpu_this();
	if (cpu->cpu_mutex_depth == 0) {
		cpu->cpu_mutex_saved_sstatus = before;
	}
	cpu->cpu_mutex_depth++;
}

static void mtx_leave() {
	if (intr_get() != 0) {
		error("mtx_leave: interrupts enabled");
	}
	cpu_t *cpu = cpu_this();
	if (cpu->cpu_mutex_depth == 0) {
		error("mtx_leave: mutex not held");
	}
	cpu->cpu_mutex_depth--;
	if (cpu->cpu_mutex_depth == 0) {
		intr_restore(cpu->cpu_mutex_saved_sstatus);
	}
}

// 原子操作接口
static inline void mtx_acquire(mutex_t *m) {
	while (atomic_lock(&m->mtx_lock_object.lo_locked) != 0)
		;
	atomic_barrier();
}

static inline void mtx_release(mutex_t *m) {
	atomic_barrier();
	atomic_unlock(&m->mtx_lock_object.lo_locked);
}

// 接口实现
void mtx_init(mutex_t *m, const char *td_name, bool debug) {
	m->mtx_lock_object.lo_name = td_name;
	m->mtx_owner = 0;
	m->mtx_debug = debug;
}

void mtx_set(mutex_t *m, const char *td_name, bool debug) {
	m->mtx_lock_object.lo_name = td_name;
	m->mtx_debug = debug;
}

/**
 * @brief 判断当前线程是否持有锁，必须在关中断的情况下调用
 */
bool mtx_hold(mutex_t *m) {
	assert(intr_get() == 0);
	// 判断锁是否被持有：
	bool locked = m->mtx_lock_object.lo_locked && m->mtx_owner == cpu_this();
	if (locked) {
		return true;
	} else {
		return cpu_this()->cpu_running != 0 &&
		       m->mtx_lock_object.lo_data == cpu_this()->cpu_running;
	}
}

// 自旋锁接口
void mtx_lock(mutex_t *m) {
	// 进入临界区，中断关闭
	mtx_enter();
	// 进入时断言：当前线程未持有锁
	assert(mtx_hold(m) == false);

	// 获取锁
	mtx_acquire(m);

	// 获取锁后，记录所有者
	m->mtx_owner = cpu_this();
	mtx_spin_debug("lock[%s] locked by %d\n", m->mtx_lock_object.lo_name, cpu_this_id());

	// 离开时中断仍然是关闭的
}

void mtx_unlock(mutex_t *m) {
	// 进入时中断仍然是关闭的

	// 解锁时断言：当前线程持有锁，且锁的使用方式为自旋锁
	assert(mtx_hold(m) == true);

	// 释放锁前，清除所有者
	m->mtx_owner = 0;
	mtx_spin_debug("unlock[%s] unlocked by %d\n", m->mtx_lock_object.lo_name, cpu_this_id());

	// 释放锁
	mtx_release(m);
	// 离开临界区后，开启中断
	mtx_leave();
}

// 睡眠锁接口
void mtx_lock_sleep(mutex_t *m) {
	// 获取自旋锁
	mtx_lock(m);
	// 检查所有权字段
	while (m->mtx_lock_object.lo_data != 0) {
		// 若自旋锁已被认领，则睡眠（睡眠时会释放自旋锁）
		sleep(m, m, "mtx_lock_sleep");
	}
	// 所有权为空，本进程认领睡眠锁
	assert(cpu_this()->cpu_running != 0);
	m->mtx_lock_object.lo_data = cpu_this()->cpu_running;
	// 认领完毕，释放互斥量
	mtx_unlock(m);
}

void mtx_unlock_sleep(mutex_t *m) {
	// 获取自旋锁
	mtx_lock(m);
	// 检查所有权字段
	assert(m->mtx_lock_object.lo_data == cpu_this()->cpu_running);
	// 释放所有权
	m->mtx_lock_object.lo_data = 0;
	// 唤醒所有等待该锁的进程
	wakeup(m);
	// 释放自旋锁
	mtx_unlock(m);
}
