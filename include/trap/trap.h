#ifndef _TRAP_H
#define _TRAP_H

#define EXCCODE_SYSCALL 8
#define EXCCODE_PAGE_FAULT 15

#include <types.h>

void kerneltrap();

void trapInitHart(void);

// User Trap
void utrap_entry();
void utrap_return();
void utrap_firstsched();

// Trap Handler
err_t page_fault_handler(u64 badva) __attribute__((warn_unused_result));

void trap_device();

void utrap_timer();
void ktrap_timer();

void trap_pgfault();

#endif
