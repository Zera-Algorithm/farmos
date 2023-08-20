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

typedef struct thread thread_t;
void trap_pgfault(thread_t *td, u64 exc_code);

#define SCAUSE_EXCEPTION 0
#define SCAUSE_INTERRUPT 1
#define INTERRUPT_TIMER 5
#define INTERRUPT_EXTERNEL 9

#endif
