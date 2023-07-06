INCLUDES := -I../../include

# modules := $(shell ls -d */)
modules := fat32 fd

.PHONY: build clean $(modules)

# 这个modules必须.PHONY，因为必须要触发子模块的make动作
$(modules):
	$(MAKE) build --directory=$@

clean:
	for d in $(modules); \
		do \
			$(MAKE) clean --directory=$$d; \
		done
	rm -rf *~ *.o *.d

# 选出所有后缀为.c或.S的文件，将其名称后缀替换为.o，作为输出目标
target = $(shell ls *.c *.S 2>/dev/null | awk '{gsub(/\.c|\.S/, ".o"); print $0}')

build: $(modules) $(target)

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

include ../../include.mk
