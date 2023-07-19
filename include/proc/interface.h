#ifndef _PROC_INTERFACE_H
#define _PROC_INTERFACE_H

#include <proc/cpu.h>
#include <proc/thread.h>
#include <types.h>

/**
 * @brief 本文件提供proc层线程的字段访问接口
 */

typedef struct thread_fs thread_fs_t;

static inline thread_fs_t *cur_proc_fs_struct() {
	return &(cpu_this()->cpu_running->td_fs_struct);
}

static inline pte_t *get_proc_pt(thread_t *td) {
	return td->td_pt;
}

static inline pte_t *cur_proc_pt() {
	return get_proc_pt(cpu_this()->cpu_running);
}

static inline proc_t *cur_proc() {
	return cpu_this()->cpu_running->td_proc;
}

#endif
