#include <lib/log.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/sleep.h>
#include <proc/thread.h>
#include <riscv.h>

#define mtx_spin_debug(...)                                                                        \
	do {                                                                                       \
		if (m->mtx_debug && (m->mtx_type & MTX_SPIN)) {                                    \
			log(MUTEX_SPIN, __VA_ARGS__);                                              \
		}                                                                                  \
	} while (0)

#define mtx_sleep_debug(...)                                                                       \
	do {                                                                                       \
		if (m->mtx_debug && (m->mtx_type & MTX_SLEEP)) {                                   \
			log(MUTEX_SLEEP, __VA_ARGS__);                                             \
		}                                                                                  \
	} while (0)

/**
 * @brief 初始化锁，设置锁的名称、类型、调试输出开关
 */
void mtx_init(mutex_t *m, const char *td_name, bool debug, u8 type) {
	m->mtx_lock_object.lo_name = td_name;
	m->mtx_owner = 0;
	m->mtx_debug = debug;
	m->mtx_type = type;
	m->mtx_depth = 0;
}

/**
 * @brief 设置锁的名称、调试输出开关
 */
void mtx_set(mutex_t *m, const char *td_name, bool debug) {
	m->mtx_lock_object.lo_name = td_name;
	m->mtx_debug = debug;
}

/**
 * @brief 判断当前线程是否持有互斥量
 */
bool mtx_hold(mutex_t *m) {
	lo_critical_enter();
	bool ret;
	if (m->mtx_type == MTX_SPIN) {
		ret = lo_acquired(&m->mtx_lock_object);
	} else if (m->mtx_type == MTX_SLEEP) {
		ret = m->mtx_owner == cpu_this()->cpu_running ? true : false;
	} else {
		error("mtx_hold: invalid mtx_type %d\n", m->mtx_type);
	}
	lo_critical_leave();
	return ret;
}

// 互斥量的锁的加锁和解锁操作
static void __mtx_lo_lock(mutex_t *m, bool need_critical) {
	// 进入或重入临界区，中断关闭
	if (need_critical) {
		lo_critical_enter();
	}
	assert(!lo_acquired(&m->mtx_lock_object));
	lo_acquire(&m->mtx_lock_object);
	// 离开时中断仍然是关闭的
}

static void __mtx_lo_unlock(mutex_t *m, bool need_critical) {
	// 进入时中断应该是关闭的
	assert(intr_get() == 0);
	// 解锁时断言：当前线程持有锁
	assert(lo_acquired(&m->mtx_lock_object));
	lo_release(&m->mtx_lock_object);
	if (need_critical) {
		lo_critical_leave();
	}
}

// 自旋互斥量接口（不检查自旋锁类型，因为睡眠锁基于自旋锁）
void mtx_lock(mutex_t *m) {
	if (m->mtx_type == MTX_SPIN) {
		// 自旋互斥量，判断是否重入
		lo_critical_enter();
		if (lo_acquired(&m->mtx_lock_object)) {
			// 重入，增加重入深度
			m->mtx_depth++;
			mtx_spin_debug("lock[%s] re-entered! (depth:%d)\n",
				       m->mtx_lock_object.lo_name, m->mtx_depth);
			// 离开临界区（自旋互斥量重入不用再套一层临界区）
			lo_critical_leave();
		} else {
			// 非重入，获取自旋锁
			__mtx_lo_lock(m, false);
			m->mtx_depth = 1;
			mtx_spin_debug("lock[%s] acquired!\n", m->mtx_lock_object.lo_name);
		}
	} else if (m->mtx_type == MTX_SLEEP) {
		// 睡眠互斥量，仅用于 sleep 结束后重新获取
		__mtx_lo_lock(m, true);
	} else {
		error("mtx_lock: invalid mtx_type %d\n", m->mtx_type);
	}
}

void mtx_unlock(mutex_t *m) {
	// 无论哪种类型的互斥量，都需要已经获取锁
	assert(lo_acquired(&m->mtx_lock_object));
	if (m->mtx_type == MTX_SPIN) {
		if (m->mtx_depth > 1) {
			// 自旋互斥量，离开重入，减少重入深度（不退出临界区，因为重入获取锁时没套临界区）
			m->mtx_depth--;
			mtx_spin_debug("lock[%s] re-leave! (depth:%d)\n",
				       m->mtx_lock_object.lo_name, m->mtx_depth);
		} else {
			// 自旋互斥量，非重入，释放锁（离开临界区）
			mtx_spin_debug("lock[%s] released!\n", m->mtx_lock_object.lo_name);
			m->mtx_depth = 0;
			__mtx_lo_unlock(m, true);
		}
	} else if (m->mtx_type == MTX_SLEEP) {
		// 睡眠互斥量，仅用于 sleep 开始时释放
		__mtx_lo_unlock(m, true);
	} else {
		error("mtx_unlock: invalid mtx_type %d\n", m->mtx_type);
	}
}

// 睡眠互斥量接口
void mtx_lock_sleep(mutex_t *m) {
	// 获取自旋锁
	assert(m->mtx_type == MTX_SLEEP);
	mtx_lock(m);
	// 已获取自旋锁，检查所有权字段
	if (m->mtx_owner == cpu_this()->cpu_running) {
		// 若睡眠锁已被自己认领，加一层嵌套，直接返回
		m->mtx_depth++;
		mtx_sleep_debug("lock[%s] already hold! (depth:%d)\n", m->mtx_lock_object.lo_name,
				m->mtx_depth);
	} else {
		// 睡眠锁未被自己认领，检查所有权字段
		while (m->mtx_owner != 0) {
			// 若自旋锁已被认领，则睡眠（睡眠时会释放自旋锁）
			sleep(m, m, "mtx_lock_sleep");
		}
		// 所有权为空，本进程认领睡眠锁
		assert(cpu_this()->cpu_running != 0);
		m->mtx_owner = cpu_this()->cpu_running;
		m->mtx_depth = 1;
	}

	// 认领完毕，释放互斥量
	mtx_unlock(m);
}

void mtx_unlock_sleep(mutex_t *m) {
	// 获取自旋锁
	assert(m->mtx_type == MTX_SLEEP);
	mtx_lock(m);
	// 检查所有权字段
	assert(m->mtx_owner == cpu_this()->cpu_running);
	// 检查重入深度
	if (m->mtx_depth > 1) {
		// 重入，减少重入深度，不唤醒
		m->mtx_depth--;
		mtx_sleep_debug("lock[%s] re-leave! (depth:%d)\n", m->mtx_lock_object.lo_name,
				m->mtx_depth);
	} else {
		// 非重入，释放所有权，重入深度置零，唤醒所有等待该锁的进程
		m->mtx_owner = 0;
		m->mtx_depth = 0;
		mtx_sleep_debug("lock[%s] released!\n", m->mtx_lock_object.lo_name);
		// 唤醒所有等待该锁的进程
		wakeup(m);
	}
	// 释放自旋锁
	mtx_unlock(m);
}
