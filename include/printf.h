#ifndef _printk_h_
#define _printk_h_

#include <stdarg.h>

void printf(const char *fmt, ...);

void _panic(const char *, int, const char *, const char *, ...)
    __attribute__((noreturn));

#define panic(...) _panic(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define panic_on(expr)                                                                             \
	do {                                                                                       \
		int r = (expr);                                                                    \
		if (r != 0) {                                                                      \
			panic("'" #expr "' returned %d", r);                                       \
		}                                                                                  \
	} while (0)

#endif /* _printk_h_ */
