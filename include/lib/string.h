#ifndef _STRING_H_
#define _STRING_H_

#include <types.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
const char *strchr(const char *s, int c);
int strcmp(const char *p, const char *q);

int memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
char *safestrcpy(char *, const char *, int);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);

#endif // _STRING_H_
