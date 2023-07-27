#ifndef _TRAP_H
#define _TRAP_H

#define EXCCODE_SYSCALL 8
#define EXCCODE_LOAD_PAGE_FAULT 13
#define EXCCODE_STORE_PAGE_FAULT 15

#include <types.h>

void kerneltrap();

void trapInitHart(void);

// User Trap
void utrap_entry();
void utrap_return();
void utrap_firstsched();

void trap_device();

void utrap_timer();
void ktrap_timer();

void trap_pgfault(thread_t *td, u64 exc_code);

#endif
