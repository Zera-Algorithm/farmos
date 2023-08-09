#include <dev/timer.h>
#include <lib/transfer.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <lib/log.h>
#include <sys/syscall.h>
#include <sys/sys_info.h>
#include <lib/string.h>
#include <dev/dtb.h>

void sys_uname(u64 upuname) {
	static utsname_t utsname = {
	    .sysname = "FarmOS",
	    .nodename = "farmos_machine",
	    .release = "0.1",
	    .version = "0.1",
	    .machine = "riscv64",
	};
	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, upuname, &utsname, sizeof(utsname));
}

/**
 * @brief 获取当前时间，当uptv和uptz不为0时，将时间和时区写入用户态
 */
void sys_gettimeofday(u64 uptv, u64 uptz) {
	timeval_t tv = time_rtc_tv();
	timezone_t tz;
	tz.tz_minuteswest = 0; // todo
	tz.tz_dsttime = 0;     // todo

	thread_t *td = cpu_this()->cpu_running;
	if (uptv)
		copy_out(td->td_pt, uptv, &tv, sizeof(tv));
	if (uptz)
		copy_out(td->td_pt, uptz, &tz, sizeof(tz));
}

// 此处不校验clockid,直接返回cpu时间
u64 sys_clock_gettime(u64 clockid, u64 tp) {
	timespec_t ts;
	if (clockid == CLOCK_REALTIME) {
		ts = time_rtc_ts();
	} else if (clockid == CLOCK_MONOTONIC) {
		ts = time_mono_ts();
	} else {
		// 其他情况
		warn("clock_gettime: clockid %d not implemented, use boot time instead\n", clockid);
		ts = time_mono_ts();
	}

	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, tp, &ts, sizeof(ts));
	log(0,"clock_gettime: %lds %ldns(%x)\n", ts.tv_sec, ts.tv_nsec, clockid);
	return 0;
}

u64 sys_geteuid() {
	warn("sys_geteuid not implemented\n");
    return 0;
}

u64 sys_getegid() {
	warn("sys_getegid not implemented\n");
    return 0;
}

u64 sys_getgid() {
	warn("sys_getgid not implemented\n");
    return 0;
}

u64 sys_getpgid() {
	warn("sys_getpgid not implemented\n");
    return 0;
}

u64 sys_setpgid(u64 pid, u64 pgid) {
	warn("sys_setpgid not implemented\n");
    return 0;
}

int sys_getrusage(int who, struct rusage *p_usage) {
	proc_t *p = cpu_this()->cpu_running->td_proc;
	proc_lock(p);
	times_t times = cpu_this()->cpu_running->td_proc->p_times;
	proc_unlock(p);
	struct rusage usage;
	memset(&usage, 0, sizeof(usage));
	usage.ru_utime.tv_sec = CLOCK_TO_USEC(times.tms_utime + times.tms_stime / 2) / 1000000ul;
	usage.ru_utime.tv_usec = CLOCK_TO_USEC(times.tms_utime + times.tms_stime / 2) % 1000000ul;
	usage.ru_stime.tv_sec = CLOCK_TO_USEC(times.tms_stime / 2) / 1000000ul;
	usage.ru_stime.tv_usec = CLOCK_TO_USEC(times.tms_stime / 2) % 1000000ul;
	u64 sum = CLOCK_TO_USEC(times.tms_utime + times.tms_stime);
	log(0, "getrusage: U: %lds %ldus / S: %lds %ldus / sum : %lds %ldus\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec, usage.ru_stime.tv_sec, usage.ru_stime.tv_usec, sum / 1000000ul, sum % 1000000ul);
	copyOut((u64)p_usage, &usage, sizeof(usage));
	return 0;
}

/**
 * @brief 向内核输出日志
 */
int sys_syslog(int priority, const char *format, ...) {
	warn("syslog not implemented\n");
	return 0;
}

int sys_sysinfo(struct sysinfo *info) {
	extern struct MemInfo memInfo;
	extern u64 pageleft;

	struct sysinfo si;
	memset(&si, 0, sizeof(si));
	si.uptime = time_mono_us() / USEC_PER_SEC;
	si.totalram = memInfo.size;
	si.freeram = pageleft * PAGE_SIZE;
	si.sharedram = 0;
	si.bufferram = 0;
	si.totalswap = 0;
	si.freeswap = 0;
	si.procs = 5;
	si.totalhigh = 0;
	si.freehigh = 0;
	si.mem_unit = 1;
	copyOut((u64)info, &si, sizeof(si));
	return 0;
}
