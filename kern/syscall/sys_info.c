#include <dev/timer.h>
#include <lib/transfer.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/time.h>
#include <sys/utsname.h>

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
	tv.tv_sec = getUSecs() / 1e6;
	tv.tv_usec = getUSecs();
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
	ts.tv_sec = getUSecs() / 1000000ul;
	ts.tv_nsec = getTime() * NSEC_PER_CLOCK;

	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, tp, &ts, sizeof(ts));
	return 0;
}
