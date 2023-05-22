#ifndef _printk_h_
#define _printk_h_

#include <stdarg.h>

// 初始化锁
void printInit(void);

// 正常输出（带锁）
void printf(const char *fmt, ...);

// 日志级别（带锁）
void _log(const char *, int, const char *, const char *, ...);
void _warn(const char *, int, const char *, const char *, ...);
void _error(const char *, int, const char *, const char *, ...) __attribute__((noreturn));

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
		if (!(expr)) {                                                                     \
			panic("'" #expr "'");                                                      \
		}                                                                                  \
	} while (0)

/**
 * @brief assert失败的同时，输出格式化字符串，帮助debug
 * @param msg：要输出的格式化字符串
 * @param ...：格式化字符串之后的可变长参数，至少要有1个
 * @example assertMsg(a == 2, "a != 2, a = %d\n", a);
 */
#define assertMsg(expr, msg, ...)                                                                  \
	do {                                                                                       \
		if (!(expr)) {                                                                     \
			panic("'" #expr "' " #msg, __VA_ARGS__);                                   \
		}                                                                                  \
	} while (0)

/**
 * @brief 输出日志，并打印所在文件、行、函数的信息
 * @param va_args：额外参数
 */
#define log(...) _log(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define warn(...) _warn(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define error(...) _error(__FILE__, __LINE__, __func__, __VA_ARGS__)

#endif /* _printk_h_ */
