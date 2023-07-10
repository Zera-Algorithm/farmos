//
// low-level driver routines for 16550a UART.
//

#include <types.h>
#include <mm/memlayout.h>
#include <riscv.h>
#include <lock/mutex.h>
#include <lib/error.h>
#include <dev/uart.h>


static inline u32 __raw_readl(const volatile void *addr)
{
	u32 val;

	asm volatile("lw %0, 0(%1)" : "=r"(val) : "r"(addr));
	return val;
}

static inline void __raw_writel(u32 val, volatile void *addr)
{
	asm volatile("sw %0, 0(%1)" : : "r"(val), "r"(addr));
}

#define __io_br()	do {} while (0)
#define __io_ar()	__asm__ __volatile__ ("fence i,r" : : : "memory");
#define __io_bw()	__asm__ __volatile__ ("fence w,o" : : : "memory");
#define __io_aw()	do {} while (0)

#define readl(c)	({ u32 __v; __io_br(); __v = __raw_readl(c); __io_ar(); __v; })
#define writel(v,c)	({ __io_bw(); __raw_writel((v),(c)); __io_aw(); })

void uart_init() {

}

int uart_getchar(void)
{
    int* uartRegRXFIFO = (int*)(HIFIVE_UART + UART_REG_RXFIFO);
	u32 ret = readl(uartRegRXFIFO);
    if (ret & UART_RXFIFO_EMPTY) {
        return -1;
    }
    // if ((ret & UART_RXFIFO_DATA) == '\r')
    //     return '\n';
    return ret & UART_RXFIFO_DATA;
}

void uart_putchar(char ch)
{
    int* uartRegTXFIFO = (int*)(HIFIVE_UART + UART_REG_TXFIFO);
	while (readl(uartRegTXFIFO) & UART_TXFIFO_FULL);
    writel(ch, uartRegTXFIFO);
}

void uart_test() {
	uart_putchar('\n');
	uart_putchar('u');
	uart_putchar('a');
	uart_putchar('r');
	uart_putchar('t');
	uart_putchar('\n');
}
