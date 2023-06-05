#ifndef __LOG_H__
#define __LOG_H__

// 日志级别定义（模块）
#define DEFAULT 1
#define LEVEL_MODULE 1
#define MM_GLOBAL 2
#define BUF_MODULE 1
#define FAT_MODULE 4

#define PROC_MODULE 1
#define PROC_GLOBAL 5

// 日志级别定义（全局）
#define LEVEL_GLOBAL 10

// 当前允许输出的日志级别
#define LOG_LEVEL 999
#define DEBUG 2

// 日志输出函数
void _log(const char *, int, const char *, const char *, ...);
void _warn(const char *, int, const char *, const char *, ...);

// 外部接口
/**
 * @brief 日志输出
 */
#define log(level, ...)                                                                            \
	do {                                                                                       \
		if ((level) >= LOG_LEVEL) {                                                        \
			_log(__FILE__, __LINE__, __func__, __VA_ARGS__);                           \
		}                                                                                  \
	} while (0)

/**
 * @brief 警告日志输出
 */
#define warn(...)                                                                                  \
	if (0)                                                                                     \
		do {                                                                               \
			_warn(__FILE__, __LINE__, __func__, __VA_ARGS__);                          \
	} while (0)

#define unwrap(expr)                                                                               \
	do {                                                                                       \
		typeof(expr) __r = (expr);                                                         \
		if (__r != 0) {                                                                    \
			warn("'" #expr "' returned %d\n", __r);                                    \
			return __r;                                                                \
		}                                                                                  \
	} while (0)

#include <lib/error.h>

#endif // __LOG_H__
