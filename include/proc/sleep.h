#ifndef _SLEEP_H
#define _SLEEP_H

#include <types.h>
void sleepProc(struct Proc *proc, u64 clocks);
void wakeupProc();
#endif