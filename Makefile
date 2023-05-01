.PHONY: clean fs.img

# 内核和用户程序的存放目录
KERN = kern
USER = user
INCLUDES = -I./include

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# 各个模块的目标文件
OBJS = \
	$(KERN)/entry.o \
	$(KERN)/start.o \
	$(KERN)/kernelvec.o \
	$(KERN)/memory.o \
	$(KERN)/main.o \
	$(KERN)/dtb.o \
	$(KERN)/proc.o \
	$(KERN)/printf.o \
	$(KERN)/string.o

QEMU		= qemu-system-riscv64
TOOLPREFIX	= riscv64-unknown-elf-

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# 编译C语言时的参数
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

# kernel
$(KERN)/kernel: clean $(OBJS) $(KERN)/kernel.ld
	$(LD) $(LDFLAGS) -T $(KERN)/kernel.ld -o $(KERN)/kernel $(OBJS)

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

# -bios default: 缺省的SBI实现(OpenSBI)
QEMUOPTS = -machine virt -bios default -kernel $(KERN)/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu: $(KERN)/kernel fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(KERN)/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
		*/*.o */*.d */*.asm */*.sym \
		kern/kernel

.PHONY: check-style fix-style

check-style: clean
	@bash utils/check-style.sh

fix-style: clean
	@bash utils/check-style.sh -f
