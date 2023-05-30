#ifndef _TRAP_H
#define _TRAP_H

#define EXCCODE_SYSCALL 8
#define EXCCODE_PAGE_FAULT 15

void kerneltrap();
void userTrap();
void userTrapReturn();
void trapInitHart(void);
#endif
