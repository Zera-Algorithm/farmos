#include <futex/futex.h>
#include <lib/log.h>
#include <sys/syscall.h>

int sys_futex(u64 uaddr, u64 futex_op, u64 val, u64 val2, u64 uaddr2, u64 val3) {
	futex_op &= (FUTEX_PRIVATE_FLAG - 1);
	switch (futex_op) {
	case FUTEX_WAIT:
		warn("called futex_wait(%p, %lu, %p)\n", uaddr, val, val2);
		return futex_wait(uaddr, val, val2);
	case FUTEX_WAKE:
		warn("called futex_wake(%p, %d)\n", uaddr, val);
		return futex_wake(uaddr, val);
	case FUTEX_REQUEUE:
		warn("called futex_requeue(%p, %p, %d, %d)\n", uaddr, uaddr2, val, val2);
		return futex_requeue(uaddr, uaddr2, val, val2);
	default:
		warn("unimplemented futex op: %d\n", futex_op);
		return -1;
	}
}

u64 sys_get_robust_list() {
	warn("unimplemented get_robust_list\n");
	return 0;
}