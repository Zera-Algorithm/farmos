#include <lib/log.h>
#include <lock/lock.h>
#include <proc/cpu.h>
#include <riscv.h>

#define atomic_lock(ptr) __sync_lock_test_and_set(ptr, 1)
#define atomic_unlock(ptr) __sync_lock_release(ptr)
#define atomic_barrier() __sync_synchronize()

// 中断使能栈操作
inline void lo_critical_enter() {
	register_t before = intr_disable();
	cpu_t *cpu = cpu_this();
	if (cpu->cpu_lk_depth == 0) {
		cpu->cpu_lk_saved_sstatus = before;
	}
	cpu->cpu_lk_depth++;
}

inline void lo_critical_leave() {
	if (intr_get() != 0) {
		error("mtx_leave: interrupts enabled");
	}
	cpu_t *cpu = cpu_this();
	if (cpu->cpu_lk_depth == 0) {
		error("mtx_leave: mutex not held");
	}
	cpu->cpu_lk_depth--;
	if (cpu->cpu_lk_depth == 0) {
		intr_restore(cpu->cpu_lk_saved_sstatus);
	}
}

// 原子操作接口
inline void lo_acquire(lock_object_t *lo) {
	assert(!lo_acquired(lo));
	while (atomic_lock(&lo->lo_locked) != 0)
		;
	atomic_barrier();
	lo->lo_data = cpu_this();
}

inline void lo_release(lock_object_t *lo) {
	assert(lo_acquired(lo));
	lo->lo_data = NULL;
	atomic_barrier();
	atomic_unlock(&lo->lo_locked);
}

inline bool lo_acquired(lock_object_t *lo) {
	assert(intr_get() == 0);
	return lo->lo_locked && lo->lo_data == cpu_this();
}