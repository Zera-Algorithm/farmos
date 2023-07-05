#include <lib/log.h>
#include <proc/cpu.h>
#include <proc/thread.h>
#include <sys/syscall.h>

void sys_exit(err_t code) {
	// 设置退出码 todo
	log(LEVEL_GLOBAL, "thread %s exit with code %d\n", cpu_this()->cpu_running->td_name, code);
	td_destroy();
}