#ifndef _SD_H
#define _SD_H

#include<types.h>

#define _AC(X,Y)        (X##Y)
#define _AT(T,X)        ((T)(X))

#define SPI_CTRL_ADDR _AC(0x10050000,UL)
#define SPI_CTRL_SIZE _AC(0x1000,UL)
#define TEST_CTRL_ADDR _AC(0x4000,UL)
#define TEST_CTRL_SIZE _AC(0x1000,UL)

#define UART_CTRL_ADDR _AC(0x10010000,UL)
#define UART_CTRL_SIZE _AC(0x1000,UL)

#define REG32(p, i)	((p)[(i) >> 2])

#define SPI_REG_SCKDIV          0x00
#define SPI_REG_SCKMODE         0x04
#define SPI_REG_CSID            0x10
#define SPI_REG_CSDEF           0x14
#define SPI_REG_CSMODE          0x18

#define SPI_REG_DCSSCK          0x28
#define SPI_REG_DSCKCS          0x2a
#define SPI_REG_DINTERCS        0x2c
#define SPI_REG_DINTERXFR       0x2e

#define SPI_REG_FMT             0x40
#define SPI_REG_TXFIFO          0x48
#define SPI_REG_RXFIFO          0x4c
#define SPI_REG_TXCTRL          0x50
#define SPI_REG_RXCTRL          0x54

#define SPI_REG_FCTRL           0x60
#define SPI_REG_FFMT            0x64

#define SPI_REG_IE              0x70
#define SPI_REG_IP              0x74


#define SPI_CSMODE_AUTO         0
#define SPI_CSMODE_HOLD         2
#define SPI_CSMODE_OFF          3

#define SPI_DIR_RX              0
#define SPI_DIR_TX              1

#define SPI_PROTO_S             0
#define SPI_PROTO_D             1
#define SPI_PROTO_Q             2

#define SPI_ENDIAN_MSB          0
#define SPI_ENDIAN_LSB          1

// 移到UART处
/* Register offsets */
#define UART_REG_TXFIFO         0x00
#define UART_REG_RXFIFO         0x04
#define UART_REG_TXCTRL         0x08
#define UART_REG_RXCTRL         0x0c
#define UART_REG_IE             0x10
#define UART_REG_IP             0x14
#define UART_REG_DIV            0x18

/* TXCTRL register */
#define UART_TXEN               0x1
#define UART_TXNSTOP            0x2
#define UART_TXWM(x)            (((x) & 0xffff) << 16)


static volatile u32 * const uart = (void *) (UART_CTRL_ADDR);

int sdTest();
int sdInit();
int sdWrite(u8 *buf, u64 startSector, u32 sectorNumber);
int sdRead(u8 *buf, u64 startSector, u32 sectorNumber);

#endif
