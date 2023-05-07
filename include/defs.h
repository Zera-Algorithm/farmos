#ifndef _DEFS_H
#define _DEFS_H

#include "types.h"
#include <stddef.h>

struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

// printf.c
void printf(const char *, ...);
void printfinit(void);

// string.c
int memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);

// proc.c
int cpuid();
struct cpu *mycpu(void);

// spinlock.c
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);

// timer.c
void timerInit();
void timerSetNextTick();

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

#endif
