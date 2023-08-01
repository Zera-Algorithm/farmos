#include <proc/thread.h>
#include <proc/cpu.h>
#include <dev/timer.h>

static inline void save_stime(proc_t *p, thread_t *td, u64 cur_us) {
    p->p_times.tms_stime += cur_us - td->td_sstart;
}

static inline void save_utime(proc_t *p, thread_t *td, u64 cur_us) {
    p->p_times.tms_utime += cur_us - td->td_ustart;
}

void utime_start(thread_t *td) {
    assert(cpu_this()->cpu_running == td);
    assert(td->td_proc != NULL);
    // 记录开始时间
    td->td_ustart = time_mono_clock();
}

void utime_end(thread_t *td) {
    assert(cpu_this()->cpu_running == td);
    assert(td->td_proc != NULL);    
    // 结束 utime 
    proc_lock(td->td_proc);
    u64 cur_us = time_mono_clock();
    save_utime(td->td_proc, td, cur_us);
    proc_unlock(td->td_proc);
}

void stime_start(thread_t *td) {
    assert(cpu_this()->cpu_running == td);
    // 开始 stime 
    td->td_sstart = time_mono_clock();
}

void stime_end(thread_t *td) {
    assert(cpu_this()->cpu_running == td);
    // 非悬垂进程时结束 stime 
    proc_lock(td->td_proc);
    u64 cur_us = time_mono_clock();
    save_stime(td->td_proc, td, cur_us);
    proc_unlock(td->td_proc);
}