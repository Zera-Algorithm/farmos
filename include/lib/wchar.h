#ifndef _WCHAR_H
#define _WCHAR_H

#include <types.h>

int wstrlen(const wchar *s);
void wstrnins(wchar *buf, const wchar *str, int len);
int wstr2str(char *dst, const wchar *src);
int str2wstr(wchar *dst, const char *src);
int strn2wstr(wchar *dst, const char *src, int n);

#endif
