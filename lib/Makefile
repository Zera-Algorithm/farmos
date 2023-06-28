INCLUDES := -I../include

# 选出所有后缀为.c或.S的文件，将其名称后缀替换为.o，作为输出目标
target = $(shell ls *.c *.S 2>/dev/null | awk '{gsub(/\.c|\.S/, ".o"); print $0}')

.PHONY: clean

include ../include.mk

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

build: $(target)

clean:
	rm -rf *~ *.o *.d
