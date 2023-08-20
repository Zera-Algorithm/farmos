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

#define GETC_EMPTY ((char)255)

static char getc_buf = GETC_EMPTY;
int cons_test_getc() {
	if (getc_buf == GETC_EMPTY) {
		getc_buf = cons_getc();
		return !(getc_buf == GETC_EMPTY);
	} else {
		return 1;
	}
}

int cons_getc() {
	char ret;
	if (getc_buf != GETC_EMPTY) {
		ret = getc_buf;
		getc_buf = GETC_EMPTY;
	} else {
	#ifdef VIRT
		ret = SBI_GETCHAR();
	#else
		ret = uart_getchar();
	#endif
	}
	if (ret == '\r') {
		return '\n';
	}
	return ret;
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
