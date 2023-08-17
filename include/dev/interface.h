#ifndef _DEV_INTERFACE_H_
#define _DEV_INTERFACE_H_

void cons_init();
void dev_init();

void cons_putc(int c);
int cons_getc();
int cons_test_getc();

typedef struct Buffer Buffer;

void disk_rw(Buffer *buf, int write);
void disk_intr();

#endif // _DEV_INTERFACE_H_
