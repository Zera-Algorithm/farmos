#ifndef _ERROR_H
#define _ERROR_H

#include <types.h>

#define E_NOPROC 1
#define E_NO_MAP 2
#define E_BAD_ELF 3

/**
 * @brief 如果传入的expr小于0，则返回该值；否则无动作。适用于函数内错误的快速返回
 * @param expr：要判断的表达式
 */
#define TRY(expr)                                                                                  \
	do {                                                                                       \
		int r = (expr);                                                                    \
		if (r < 0) {                                                                       \
			loga("TRY error\n");                                                       \
			return r;                                                                  \
		}                                                                                  \
	} while (0)

#endif
