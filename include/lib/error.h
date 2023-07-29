#ifndef _ERROR_H
#define _ERROR_H

#include <types.h>

#define IGNORE_ASSERT 1

#define E_NOPROC 1
#define E_NO_MAP 2
#define E_BAD_ELF 3
#define E_UNKNOWN_FS 4
#define E_DEV_ERROR 5
#define E_NOT_FOUND 6
#define E_BAD_PATH 7
#define E_FILE_EXISTS 8
#define E_EXCEED_FILE 9

void _error(const char *, int, const char *, const char *, ...) __attribute__((noreturn));

/**
 * @brief 错误日志输出
 */
#define error(...)                                                                                 \
	do {                                                                                       \
		_error(__FILE__, __LINE__, __func__, __VA_ARGS__);                                 \
	} while (0)

#define panic(...) _error(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define panic_on(expr)                                                                             \
	do {                                                                                       \
		int r = (expr);                                                                    \
		if (r != 0) {                                                                      \
			panic("'" #expr "' returned %d\n", r);                                     \
		}                                                                                  \
	} while (0)

#define assert(expr)                                                                               \
	do {                                                                                       \
		if (!IGNORE_ASSERT && (!(expr))) {                                                                     \
			panic("'" #expr "'");                                                      \
		}                                                                                  \
	} while (0)

#endif
