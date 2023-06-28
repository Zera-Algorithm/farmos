#include <lib/log.h>
#include <lock/mutex.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <riscv.h>

#define atomic_lock(ptr) __sync_lock_test_and_set(ptr, 1)
#define atomic_unlock(ptr) __sync_lock_release(ptr)
#define atomic_barrier() __sync_synchronize()

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

// 接口实现

void mtx_init(mutex_t *m, const char *name, bool debug) {
	m->mtx_lock_object.lo_name = name;
	m->mtx_owner = 0;
	m->mtx_debug = debug;
}

void mtx_set(mutex_t *m, const char *name, bool debug) {
	m->mtx_lock_object.lo_name = name;
	m->mtx_debug = debug;
}

void mtx_lock(mutex_t *m) {
	mtx_enter();
	// 关闭中断后，进入临界区
	if (mtx_hold(m)) {
		warn("mtx_lock: mutex already held");
		while (1) {
			/* code */
		}
	}
	while (atomic_lock(&m->mtx_lock_object.lo_locked) != 0)
		;
	atomic_barrier();
	m->mtx_owner = cpu_this();
	if (m->mtx_debug) {
		log(MTX, "lock[%s] locked by %d\n", m->mtx_lock_object.lo_name, cpu_this_id());
	}
	// 离开时中断仍然是关闭的
}

void mtx_unlock(mutex_t *m) {
	// 进入时中断仍然是关闭的
	if (!mtx_hold(m)) {
		warn("mtx_unlock: mutex not held\n");
		while (1) {
			/* code */
		}
	}
	if (m->mtx_debug) {
		log(MTX, "unlock[%s] unlocked by %d\n", m->mtx_lock_object.lo_name, cpu_this_id());
	}
	m->mtx_owner = 0;
	// log(999,"mtx_unlock[%d] called by %d\n", m, cpu_this_id());
	atomic_barrier();
	atomic_unlock(&m->mtx_lock_object.lo_locked);

	// 离开临界区后，开启中断
	mtx_leave();
}

/**
 * @brief 判断当前线程是否持有锁，必须在关中断的情况下调用
 */
bool mtx_hold(mutex_t *m) {
	assert(intr_get() == 0);
	int r = ((m->mtx_lock_object.lo_locked) && (m->mtx_owner == cpu_this()));
	return r;
}