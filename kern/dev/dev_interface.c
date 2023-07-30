#include <dev/interface.h>
#include <dev/virtio.h>
#include <dev/sbi.h>
#include <dev/uart.h>

void dev_test() {
    // virtio_disk_test();
}

void dev_init() {
    virtio_disk_init();
    uart_init();
    dev_test();
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
    virtio_disk_rw(buf, write);
}

void disk_intr() {
    virtio_disk_intr();
}