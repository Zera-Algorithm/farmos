# 内核和用户程序的存放目录
KERN = kern
USER = user
LIB  = lib
KERNEL_LD := linker/kernel.ld
INCLUDES = -I./include

include include.mk

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# 编译好待链接的目标文件
OBJS := $(KERN)/*/*.o $(LIB)/*.o
modules := $(KERN) $(LIB)

.PHONY: clean $(modules) fs.img

# 生成 kernel，并将其反汇编到kernel.asm
$(KERN)/kernel: clean $(modules) $(KERNEL_LD)
	$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $(KERN)/kernel $(OBJS)
	$(OBJDUMP) -S $(KERN)/kernel > $(KERN)/kernel.asm

$(modules):
	$(MAKE) build --directory=$@

# TODO: 需要实现。现在仅仅是使用了一个使用mkfs创建的默认镜像
fs.img:
	dd if=/dev/zero of=fs.img bs=16k count=1024
	mkfs.vfat -F 12 fs.img

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

# 可以暂时不需要镜像文件
# qemu: $(KERN)/kernel fs.img
qemu: $(KERN)/kernel
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

# 可以暂时不需要镜像文件
# qemu-gdb: $(KERN)/kernel .gdbinit fs.img
qemu-gdb: $(KERN)/kernel .gdbinit
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
		*/*/*.o */*/*.d */*/*.asm */*/*.sym \
		kern/kernel

.PHONY: check-style fix-style

check-style: clean
	@bash scripts/check-style.sh

fix-style: clean
	@bash scripts/check-style.sh -f
