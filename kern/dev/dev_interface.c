#include <dev/interface.h>
#include <dev/virtio.h>
#include <dev/sbi.h>
#include <dev/uart.h>
#include <lib/printf.h>
#include <dev/sd.h>
#include <fs/buf.h>

void dev_test() {
    // virtio_disk_test();
    sdTest();
}

void cons_init() {
    printInit();
    uart_init();
}

void dev_init() {
    #ifdef QEMU
    // virtio_disk_init();
    #endif
    sdInit();
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
    // virtio_disk_rw(buf, write);
    u64 sector = buf->blockno * (BUF_SIZE / 512);
    u8 * buffer = (u8 *)buf->data;
    if (write) {
        sdWrite(buffer, sector, 1);
    } else {
        sdRead(buffer, sector, 1);
    }
}

void disk_intr() {
    virtio_disk_intr();
}