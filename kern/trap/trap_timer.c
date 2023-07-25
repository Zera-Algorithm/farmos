#include <dev/timer.h>
#include <futex/futex.h>
#include <lib/log.h>
#include <proc/cpu.h>
#include <proc/nanosleep.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <trap/trap.h>

void ktrap_timer() {
	log(DEFAULT, "timer interrupt on CPU %d!\n", cpu_this_id());
	handler_timer_int();
	nanosleep_check();
	futexevent_check();
}

void utrap_timer() {
	// 时钟中断
	log(LEVEL_MODULE, "Timer Int On Hart %d\n", cpu_this_id());
	// 先设置下次时钟中断的触发时间，再进行调度
	ktrap_timer();
	yield();
}
