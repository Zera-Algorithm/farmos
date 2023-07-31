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
	timeval_t tv;
	timezone_t tz;
	tv.tv_sec = getUSecs() / 1000000ul;
	tv.tv_usec = getUSecs() % 1000000ul;
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
	ts.tv_sec = getTime() / 10000000ul;
	ts.tv_nsec = (getTime() * NSEC_PER_CLOCK) % 1000000000ul;

	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, tp, &ts, sizeof(ts));
	// warn("clock_gettime: %lds %ldns\n", ts.tv_sec, ts.tv_nsec);
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
	struct rusage usage;
	times_t *times = &cpu_this()->cpu_running->td_proc->p_times;
	memset(&usage, 0, sizeof(usage));
	usage.ru_utime.tv_sec = times->tms_utime / 1000000ul;
	usage.ru_utime.tv_usec = times->tms_utime % 1000000ul;
	usage.ru_stime.tv_sec = times->tms_stime / 1000000ul;
	usage.ru_stime.tv_usec = times->tms_stime % 1000000ul;
	// warn("getrusage: U: %lds %ldus / S: %lds %ldus\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec, usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
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
	si.uptime = getUSecs() / 1000000ul;
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
