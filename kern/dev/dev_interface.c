#include <dev/interface.h>
#include <dev/sbi.h>
#include <dev/sd.h>
#include <dev/uart.h>
#include <dev/virtio.h>
#include <fs/buf.h>
#include <lib/printf.h>
#include <lib/log.h>

void dev_test() {
#ifdef SIFIVE
	sdTest();
#endif
}

void cons_init() {
	printInit();
	uart_init();
}

void dev_init() {
#ifdef SIFIVE
	sdInit();
#else
	virtio_disk_init();
#endif
}

void cons_putc(int c) {
	// SBI_PUTCHAR(c);
	uart_putchar(c);
}

int cons_getc() {
	// return SBI_GETCHAR();
	return uart_getchar();
}

void disk_rw(Buffer *buf, int write) {
#ifdef SIFIVE
	sd_rw(buf, write);
#else
	virtio_disk_rw(buf, write);
#endif
}

void disk_intr() {
#ifdef SIFIVE
    panic("sd card not support disk_intr");
#else
	virtio_disk_intr();
#endif
}