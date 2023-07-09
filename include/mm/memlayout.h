#ifndef _MEMLAYOUT_H
#define _MEMLAYOUT_H
/**
 * FarmOS 的虚拟内存布局：
 * 
 * |------------------| MAXVA
 * |     PAGE_SIZE    | 
 * |------------------| TRAMPOLINE
 * |     PAGE_SIZE    |
 * |------------------| TRAPFRAME
 * |     PAGE_SIZE    |
 * |------------------| STACKTOP/USTACKTOP
 * |    STACK_SIZE    |
 * |------------------| TD_KSTACK/USTACK
 * |                  |
 * 
 * 用户空间中，从 0 开始为用户代码和数据，以及用户堆
 * 内核空间中，从 0x80200000 开始为内核代码和数据
 */

// FarmOS 物理页说明
#define PAGE_SHIFT (12ull)	    // 基页大小为 4KB
#define PAGE_SIZE (1 << PAGE_SHIFT) // 基页大小为 4KB
#define PGROUNDUP(a) (((a) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PAGE_SIZE - 1))

// 最大虚拟地址
#ifndef MAXVA
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#endif // !MAXVA

// FarmOS 虚拟内存布局
#define TRAMPOLINE (MAXVA - PAGE_SIZE)
#define TRAPFRAME (TRAMPOLINE - PAGE_SIZE)
#define STACKTOP (TRAPFRAME - PAGE_SIZE)
#define USTACKTOP STACKTOP

// 内核初始化栈部分（静态数组）
#define KSTACKSIZE (4 * PAGE_SIZE)

// 内核页表中，线程的内核栈部分
#define TD_KSTACK_PAGE_NUM 4				// 内核栈占用的页数
#define TD_KSTACK_SIZE (TD_KSTACK_PAGE_NUM * PAGE_SIZE) // 内核栈占用的大小
#define TD_KSTACK(p) (STACKTOP - ((p) + 1) * (TD_KSTACK_SIZE + PAGE_SIZE))

// 用户页表中，线程的用户栈部分
#define TD_USTACK_PAGE_NUM 4				// 用户栈占用的页数
#define TD_USTACK_SIZE (TD_USTACK_PAGE_NUM * PAGE_SIZE) // 用户栈占用的大小
#define TD_USTACK (USTACKTOP - (TD_USTACK_SIZE + PAGE_SIZE))
#define TD_TEMPUSTACK (TD_USTACK - TD_USTACK_SIZE - PAGE_SIZE)
#define TD_TEMPUSTACK_OFFSET (TD_USTACK - TD_TEMPUSTACK)

// 内核的起始位置
#define KERNBASE 0x80200000ul

// 可访问内存的起始位置
#define MEMBASE 0x80000000ul

// BELOW TO BE CLASSIFIED (TODO)

// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtio disk
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000ul
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000ul
#define VIRTIO0_IRQ 1

// RTC实时时钟地址映射
#define RTC_BASE 0x101000ul

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000ul
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000ul
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)


#endif // !_MEMLAYOUT_H
