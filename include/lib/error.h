#define E_NOPROC 1

/**
 * @brief 如果传入的expr小于0，则返回该值；否则无动作。适用于函数内错误的快速返回
 * @param expr：要判断的表达式
 */
#define TRY(expr)                                                                                  \
	do {                                                                                       \
		int r = (expr);																	\
		if (r < 0) {                                                                  \
			return r;                                                             \
		}                                                                                  \
	} while (0)
