#ifndef _DEFS_H
#define _DEFS_H

#include "types.h"
#include <stddef.h>

struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct Proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

// printf.c
void printf(const char *, ...);
void printfinit(void);

// Proc.c
int cpuid();
struct cpu *mycpu(void);

// spinlock.c

int holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);

// timer.c
void timerInit();
void timerSetNextTick();

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

#endif
