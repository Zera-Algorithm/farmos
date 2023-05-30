#ifndef _printk_h_
#define _printk_h_

#include <proc/proc.h>
#include <stdarg.h>

// 初始化锁
void printInit(void);

// 正常输出（带锁）
void printf(const char *fmt, ...);

// 输出到字符串
void sprintf(char *buf, const char *fmt, ...);

// 条件输出（带锁）
#define printfIf(isLog, ...)                                                                       \
	if ((isLog))                                                                               \
	printf(__VA_ARGS__)

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

// 模块级日志
#define LEVEL_MODULE 1
#define MM_GLOBAL 2
#define BUF_MODULE 3
#define FAT_MODULE 4
// 全局级日志
#define LEVEL_GLOBAL 10

// 定义的日志等级：越大，打印的信息越重要
#define LOG_LEVEL 10
#define log(level, ...)                                                                            \
	if ((level) >= LOG_LEVEL)                                                                  \
	_log(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define warn(...) _warn(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define error(...) _error(__FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief 旧版log
 */
#define loga(...) log(1, __VA_ARGS__)

/**
 * @brief 条件输出日志，适用于隐藏调试信息
 * @param isLog 若为真，则输出日志
 * @param va_args 额外参数
 */
#define logIf(isLog, ...)                                                                          \
	if ((isLog))                                                                               \
	_log(__FILE__, __LINE__, __func__, __VA_ARGS__)

void printReg(struct trapframe *tf);

#endif /* _printk_h_ */
