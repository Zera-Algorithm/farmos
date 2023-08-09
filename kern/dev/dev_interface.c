#include <dev/interface.h>
#include <dev/sbi.h>
#include <dev/sd.h>
#include <dev/uart.h>
#include <dev/virtio.h>
#include <fs/buf.h>
#include <lib/printf.h>
#include <lib/log.h>
#include <param.h>

void dev_test() {
#ifdef FEATURE_DISK_SD
	sdTest();
#endif
}

void cons_init() {
	SBI_PUTCHAR('s');
	SBI_PUTCHAR('b');
	SBI_PUTCHAR('i');
	SBI_PUTCHAR(' ');
	SBI_PUTCHAR('o');
	SBI_PUTCHAR('k');
	SBI_PUTCHAR('\n');
	printInit();
	uart_init();
}

void dev_init() {
#ifdef FEATURE_DISK_SD
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
#ifdef FEATURE_DISK_SD
	sd_rw(buf, write);
#else
	virtio_disk_rw(buf, write);
#endif
}

void disk_intr() {
#ifdef FEATURE_DISK_SD
    panic("sd card not support disk_intr");
#else
	virtio_disk_intr();
#endif
}
