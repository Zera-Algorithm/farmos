#include <sys/syscall.h>
#include <lib/log.h>

u64 sys_sched_getaffinity() {
    warn("sched_getaffinity not implemented\n");
    return 0;
}

u64 sys_sched_setaffinity() {
    warn("sched_setaffinity not implemented\n");
    return 0;
}

u64 sys_sched_getscheduler() {
    warn("sched_getscheduler not implemented\n");
    return 0;
}

u64 sys_sched_setscheduler() {
    warn("sched_setscheduler not implemented\n");
    return 0;
}

u64 sys_sched_getparam() {
    warn("sched_getparam not implemented\n");
    return 0;
}