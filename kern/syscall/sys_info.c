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

void sys_gettimeofday(u64 uptv, u64 uptz) {
	timeval_t tv;
	timezone_t tz;
	tv.tv_sec = getUSecs() / 1e6;
	tv.tv_usec = getUSecs();
	tz.tz_minuteswest = 0; // todo
	tz.tz_dsttime = 0;     // todo

	thread_t *td = cpu_this()->cpu_running;
	copy_out(td->td_pt, uptv, &tv, sizeof(tv));
	copy_out(td->td_pt, uptz, &tz, sizeof(tz));
}