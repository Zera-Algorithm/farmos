INCLUDES		:= -I./include/ -I../include
BINTOC			:= ../scripts/binToC.py
USER_LIB		:= libMain.o syscallLib.o clone.o stdio.o stdlib.o string.o

USER_TARGET		:= test_while.x test_sleep.x test_exit.x test_clone.x test_init.x \
					test_argv.x test_execve.x test_pipe.x test_printf.x test_mmap.x

.PHONY: build clean

build: $(BINTOC) $(USER_LIB) $(USER_TARGET)

include ../include.mk

# .b.c：二进制数组的C语言表示
# .x：编译好的二进制数组目标文件，可以链接进内核
%.x: %.b.c
	$(CC) $(CFLAGS) -c -o $@ $<

# .b：链接好的用户二进制可执行文件
%.b.c: %.b
	$(PYTHON) $(BINTOC) $< $@

# 链接用户程序的目标文件，得到.b文件，并反汇编到asm文件
%.b: entry.o %.o $(USER_LIB)
	$(LD) -o $@ $(LDFLAGS) -G 0 -static -n -nostdlib -T ../linker/user.ld $^
	$(OBJDUMP) -D $@ > $@.asm

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf *.o *~ *.d *.x *.asm *.b *.b.c
