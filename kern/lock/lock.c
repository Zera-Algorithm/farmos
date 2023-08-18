#include <lib/log.h>
#include <lock/lock.h>
#include <proc/cpu.h>
#include <riscv.h>
#include <lock/mutex.h>

#define atomic_lock(ptr) __sync_lock_test_and_set(ptr, 1)
#define atomic_unlock(ptr) __sync_lock_release(ptr)
#define atomic_barrier() __sync_synchronize()

// 中断使能栈操作
inline void lo_critical_enter(mutex_t *m) {
	register_t before = intr_disable();
	cpu_t *cpu = cpu_this();
	if (cpu->cpu_lk_depth == 0) {
		cpu->cpu_lk_saved_sstatus = before;
	}
#ifdef LOCK_DEPTH_DEBUG
	// 记录加的锁的指针
	cpu_this()->cpu_lks[cpu->cpu_lk_depth] = m;
#endif
	cpu->cpu_lk_depth++;
}

inline void print_lock_info() {
	cpu_t *cpu = cpu_this();
	for (int i = 0; i < cpu->cpu_lk_depth; i++) {
		log(999, "lock %d/%d:(0x%08lx) %s\n", i, cpu->cpu_lk_depth, cpu->cpu_lks[i], cpu->cpu_lks[i]->mtx_lock_object.lo_name);
	}
}

inline void lo_critical_leave(mutex_t *m) {
	if (intr_get() != 0) {
		error("mtx_leave: interrupts enabled");
	}
	cpu_t *cpu = cpu_this();
	if (cpu->cpu_lk_depth == 0) {
		error("mtx_leave: mutex not held");
	}
#ifdef LOCK_DEPTH_DEBUG
	// 记录解的锁的指针
	if (cpu->cpu_lks[cpu->cpu_lk_depth - 1] == m) {
		cpu->cpu_lks[cpu->cpu_lk_depth - 1] = NULL;
	} else {
		int isfind = 0;
		for (int i = 0; i < cpu->cpu_lk_depth; i++) {
			if (cpu->cpu_lks[i] == m) {
				cpu->cpu_lks[i] = cpu->cpu_lks[cpu->cpu_lk_depth - 1];
				cpu->cpu_lks[cpu->cpu_lk_depth - 1] = NULL;
				isfind = 1;
				break;
			}
		}
		assert(isfind);
	}
#endif
	cpu->cpu_lk_depth--;
	if (cpu->cpu_lk_depth == 0) {
		intr_restore(cpu->cpu_lk_saved_sstatus);
	}
}

#define THRESHOLD_LOCK 100000000ul

// 原子操作接口
inline void lo_acquire(lock_object_t *lo) {
	u64 cnt = 0;
	assert(!lo_acquired(lo));
	while (atomic_lock(&lo->lo_locked) != 0) {
		cnt += 1;
		// 在自旋锁长时间获取不到时，打印信息
		if (cnt >= THRESHOLD_LOCK && cnt % THRESHOLD_LOCK == 0) {
			log(9999, "wait too long of spinlock[%s]\n", lo->lo_name);
		}
	}
	atomic_barrier();
	lo->lo_data = cpu_this();
}

inline bool lo_try_acquire(lock_object_t *lo) {
	assert(!lo_acquired(lo));
	if (atomic_lock(&lo->lo_locked) != 0) {
		return false;
	}
	atomic_barrier();
	lo->lo_data = cpu_this();
	return true;
}

inline void lo_release(lock_object_t *lo) {
	assert(lo_acquired(lo));
	lo->lo_data = NULL;
	atomic_barrier();
	atomic_unlock(&lo->lo_locked);
}

inline bool lo_acquired(lock_object_t *lo) {
	if ((u64)lo <= 0x80000000ul) {
		asm volatile("nop");
	}
	assert(intr_get() == 0);
	return lo->lo_locked && lo->lo_data == cpu_this();
}
