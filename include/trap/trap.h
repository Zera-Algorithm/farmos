#ifndef _TRAP_H
#define _TRAP_H

#define EXCCODE_SYSCALL 8
#define EXCCODE_PAGE_FAULT 15

void kerneltrap();

void trapInitHart(void);

// User Trap
void utrap_entry();
void utrap_return();
void utrap_firstsched();

#endif
