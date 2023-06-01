#ifndef _WCHAR_H
#define _WCHAR_H

#include <types.h>

int wstrlen(const wchar *s);
void wstrnins(wchar *buf, const wchar *str, int len);
int wstr2str(char *dst, const wchar *src);

#endif
