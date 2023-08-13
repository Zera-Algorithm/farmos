#ifndef _CONSOLE_H
#define _CONSOLE_H
#include <types.h>

int readConsoleAlloc();
int writeConsoleAlloc();
int errorConsoleAlloc();

int console_read(u64 buf, u64 n);
int console_write(u64 buf, u64 n);

#endif
