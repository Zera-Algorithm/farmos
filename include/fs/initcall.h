#ifndef _INITCALL_H
#define _INITCALL_H

/**
 * @brief 定义fs层的初始化函数
 */

typedef int (*initcall_t)(void);

// 加used是为了避免被清理掉
// 加section是为了放到initcall_fs段中

/**
 * @brief 定义fs层的初始化函数，以在初始化时调用
 */
#define fs_initcall(fn)                                                                                                \
	static initcall_t __initcall_##fn __attribute__((used)) __attribute__((__section__(".initcall_fs"))) = fn

#endif
