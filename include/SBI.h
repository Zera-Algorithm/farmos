#ifndef _SBI_H
#define _SBI_H
#include "types.h"

struct sbiret {
	uint64 error;
	uint64 value;
};

// num表示SBI标号, a0是第一个参数，也是返回值
// a1, a2是第2、3个参数
#define SBI_LEGACY_ECALL(__num, __a0, __a1, __a2)                                                  \
	({                                                                                         \
		register unsigned long a0 asm("a0") = (unsigned long)(__a0);                       \
		register unsigned long a1 asm("a1") = (unsigned long)(__a1);                       \
		register unsigned long a2 asm("a2") = (unsigned long)(__a2);                       \
		register unsigned long a7 asm("a7") = (unsigned long)(__num);                      \
		asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");           \
		a0;                                                                                \
	})

#define SBI_LEGACY_PUTCHAR_NUM 0x01
#define SBI_LEGACY_GETCHAR_NUM 0x02

#define SBI_PUTCHAR(ch) SBI_LEGACY_ECALL(SBI_LEGACY_PUTCHAR_NUM, ch, 0, 0)
// 成功返回读到的字符，失败返回-1
#define SBI_GETCHAR() SBI_LEGACY_ECALL(SBI_LEGACY_GETCHAR_NUM, 0, 0, 0)

#define SBI_ECALL(__eid, __fid, __a0, __a1, __a2, __a3, __a4)                                      \
	({                                                                                         \
		unsigned long error = 0, value = 0;                                                \
		register unsigned long a0 asm("a0") = (unsigned long)(__a0);                       \
		register unsigned long a1 asm("a1") = (unsigned long)(__a1);                       \
		register unsigned long a2 asm("a2") = (unsigned long)(__a2);                       \
		register unsigned long a3 asm("a3") = (unsigned long)(__a3);                       \
		register unsigned long a4 asm("a4") = (unsigned long)(__a4);                       \
		register unsigned long a6 asm("a6") = (unsigned long)(__fid);                      \
		register unsigned long a7 asm("a7") = (unsigned long)(__eid);                      \
		asm volatile("ecall\n"                                                             \
			     "mv %[error], a0\n"                                                   \
			     "mv %[value], a1\n"                                                   \
			     : "+r"(a0), "+r"(a1), [error] "=r"(error), [value] "=r"(value)        \
			     : "r"(a2), "r"(a3), "r"(a4), "r"(a6), "r"(a7)                         \
			     : "memory");                                                          \
		(struct sbiret){error, value};                                                     \
	})

#define SBI_TIMER_EID 0x54494D45
#define SBI_SET_TIMER_FID 0
#define SBI_IPI_EID 0x735049
#define SBI_IPI_SEND_IPI_FID 0
#define SBI_RFENCE_EID 0x52464E43
#define SBI_RFENCE_FENCEI_FID 0
#define SBI_RFENCE_SFENCE_VMA_FID 1
#define SBI_RFENCE_SFENCE_VMA_ASID_FID 2
#define SBI_HSM_EID 0x48534D
#define SBI_HSM_HART_START_FID 0
#define SBI_HSM_HART_STOP_FID 1
#define SBI_HSM_HART_GET_STATUS_FID 2
#define SBI_HSM_HART_SUSPEND_FID 3
#define SBI_SYSTEM_RESET_EID 0x53525354
#define SBI_SYSTEM_RESET_FID 0

// 这里是HSM定义的一些Hart的状态
#define STARTED 0
#define STOPPED 1
#define START_PENDING 2
#define STOP_PENDING 3
#define SUSPENDED 4
#define SUSPEND_PENDING 5
#define RESUME_PENDING 6

// 对于下面的宏函数，其返回值是sbiret结构体，定义在本文件的顶部


/* 执行此函数后，会在stime_value **时刻** 触发一次时钟中断
 * 注意是时刻而不是时间间隔，因此中断时需要将这个值刷新为 "当前时间+interval"
 * 这个时钟是核内时钟，不同核的时钟不同
 * virt下Riscv的时钟频率为10mHz，也就是当前时间一秒加10^7
 */
#define SBI_SET_TIMER(stime_value)                                                                 \
	SBI_ECALL(SBI_TIMER_EID, SBI_SET_TIMER_FID, stime_value, 0, 0, 0, 0)

#define SBI_SEND_IPI(hart_mask, hart_mask_base)                                                    \
	SBI_ECALL(SBI_IPI_EID, SBI_IPI_SEND_IPI_FID, hart_mask, hart_mask_base, 0, 0, 0)

#define SBI_RFENCE_FENCEI(hart_mask, hart_mask_base)                                               \
	SBI_ECALL(SBI_RFENCE_EID, SBI_RFENCE_FENCEI_FID, hart_mask, hart_mask_base, 0, 0, 0)

#define SBI_RFENCE_SFENCE_VMA(hart_mask, hart_mask_base, start_addr, size)                         \
	SBI_ECALL(SBI_RFENCE_EID, SBI_RFENCE_SFENCE_VMA_FID, hart_mask, hart_mask_base,            \
		  start_addr, size, 0)

#define SBI_RFENCE_SFENCE_VMA_ASID(hart_mask, hart_mask_base, start_addr, size, asid)              \
	SBI_ECALL(SBI_RFENCE_EID, SBI_RFENCE_SFENCE_VMA_FID, hart_mask, hart_mask_base,            \
		  start_addr, size, asid)

// 启动hartid。start_addr是该hart在S态启动时的初始地址，opaque是传递给hart的第二个参数（a1）
#define SBI_HART_START(hartid, start_addr, opaque)                                                 \
	SBI_ECALL(SBI_HSM_EID, SBI_HSM_HART_START_FID, hartid, start_addr, opaque, 0, 0)

// 请求SBI关闭发起调用的核。
// The sbi_hart_stop() must be called with the supervisor-mode interrupts disabled
#define SBI_HART_STOP() SBI_ECALL(SBI_HSM_EID, SBI_HSM_HART_STOP_FID, 0, 0, 0, 0, 0)

// 获取hartid的状态，错误存储在error, 返回值存储在value
#define SBI_HART_GET_STATUS(hartid)                                                                \
	SBI_ECALL(SBI_HSM_EID, SBI_HART_GET_STATUS_FID, hartid, 0, 0, 0, 0)

#define SBI_HART_SUSPEND(suspend_type, resume_addr, opaque)                                        \
	SBI_ECALL(SBI_HSM_EID, SBI_HSM_HART_SUSPEND_FID, suspend_type, resume_addr, opaque, 0, 0)

#define SBI_SYSTEM_RESET(reset_type, reset_reason)                                                 \
	SBI_ECALL(SBI_SYSTEM_RESET_EID, SBI_SYSTEM_RESET_FID, reset_type, reset_reason, 0, 0, 0)
#endif