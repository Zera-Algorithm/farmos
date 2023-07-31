#include <dev/timer.h>
#include <lib/log.h>
#include <proc/cpu.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <trap/trap.h>
#include <proc/tsleep.h>
#include <signal/itimer.h>

void ktrap_timer() {
	log(DEFAULT, "timer interrupt on CPU %d!\n", cpu_this_id());
	handler_timer_int();
	tsleep_check();
	itimer_check();
}

void utrap_timer() {
	// 时钟中断
	// log(LEVEL_GLOBAL, "Timer Int On Hart %d\n", cpu_this_id());
	// 先设置下次时钟中断的触发时间，再进行调度
	ktrap_timer();
	yield();
}
