#ifndef _TYPES_H
#define _TYPES_H

// FarmOS 有符号整型
typedef char i8;
typedef short i16;
typedef int i32;
typedef long i64;

// FarmOS 无符号整型
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

// FarmOS 布尔类型
typedef enum { false = 0, true = 1 } bool;

// FarmOS 标准定义
#define NULL ((void *)0)

// FarmOS 其它
typedef u64 size_t;
typedef i32 err_t; // err_t允许返回负数，不能用u开头的类型
typedef i64 off_t;
typedef u64 pte_t;
typedef u64 register_t;
typedef u64 ptr_t;
typedef u32 mode_t;
typedef i64 fileid_t;
typedef i64 clock_t;
typedef i64 time_t;
typedef u64 suseconds_t;
typedef u32 pid_t;
typedef u32 uid_t;
typedef u64 sigval_t;
typedef u32 socklen_t;
typedef u64 rlim_t;

// FarmOS 页表项
typedef pte_t Pte;

// FarmOS 结构体宏

#define offsetof(type, member) ((size_t) & ((type *)0)->member)

#define container_of(ptr, type, member)                                                            \
	({                                                                                         \
		const typeof(((type *)0)->member) *__mptr = (ptr);                                 \
		(type *)((char *)__mptr - offsetof(type, member));                                 \
	})

#define rangeof(type, start, end) (offsetof(type, end) - offsetof(type, start))

// Old Defination
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

// wchar（占两位的char）
typedef u16 wchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

/**
 * @brief 返回两数中最小的那个
 * @note 这里我们使用typeof(_a) __a = (_a); 是为了避免多次执行_a（_a可能是一个有后效的函数）
 */
#define MIN(_a, _b)                                                                                \
	({                                                                                         \
		typeof(_a) __a = (_a);                                                             \
		typeof(_b) __b = (_b);                                                             \
		__a <= __b ? __a : __b;                                                            \
	})

/**
 * @brief 返回两数中最大的那个
 * @note 这里我们使用typeof(_a) __a = (_a); 是为了避免多次执行_a（_a可能是一个有后效的函数）
 */
#define MAX(_a, _b)                                                                                \
	({                                                                                         \
		typeof(_a) __a = (_a);                                                             \
		typeof(_b) __b = (_b);                                                             \
		__a >= __b ? __a : __b;                                                            \
	})

#define ROUNDUP(a, x) (((a) + (x)-1) & ~((x)-1))

#endif
