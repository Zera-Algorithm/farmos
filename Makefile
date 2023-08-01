# 内核和用户程序的存放目录
KERN = kern
USER = user
LIB  = lib
KERNEL_LD := linker/kernel.ld
INCLUDES := -I./include
KERNEL_ELF := kernel-qemu

include include.mk

# kernel的二进制文件
KERNEL_BIN := hifive.bin
KERNEL_UIMAGE := hifive.uImage

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# 编译好待链接的目标文件
# 这里必须改，不然入口_entry不在0x80200000
OBJS := $(KERN)/*/*.o $(LIB)/*.o $(USER)/*.x $(KERN)/*/*/*.o
modules := $(KERN) $(LIB) $(USER)

.PHONY: all clean $(modules) fs.img sdcard.img

all: $(KERNEL_ELF)

# 生成 kernel，并将其反汇编到kernel.asm
$(KERNEL_ELF): $(modules) $(KERNEL_LD)
	$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $(KERNEL_ELF) $(OBJS)
	$(OBJDUMP) -xS $(KERNEL_ELF) > $(KERN)/kernel.asm

# modules即为 kern 各目录的生成产物，一般是一些 .o 文件
$(modules):
	$(MAKE) build --directory=$@

objcopy: $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

mkimage: objcopy
	mkimage -A riscv -O linux -C none -a 0x80200000 -e 0x80200000 \
		-n farmos -d $(KERNEL_BIN) $(KERNEL_UIMAGE)

# TODO: 需要实现。现在仅仅是使用了一个使用mkfs创建的默认镜像
fs.img:
	cp backup_fs.img fs.img
	# dd if=/dev/zero of=fs.img bs=16k count=1024
	# mkfs.vfat -F 12 fs.img

sdcard.img:
	cp sdcard.img fs.img
	qemu-img resize fs.img 64M

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

# 可以暂时不需要镜像文件
# qemu: $(KERNEL_ELF)
fsrun: $(KERNEL_ELF) fs.img
	$(QEMU) $(QEMUOPTS)

# 以sd卡运行
sdrun: $(KERNEL_ELF) sdcard.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(KERNEL_ELF) sdcard.img
	$(QEMU) $(QEMUOPTS) -S -s

clean:
	for d in $(modules); \
		do \
			$(MAKE) --directory=$$d clean; \
		done
	rm -f $(KERNEL_ELF)

.PHONY: check-style fix-style

check-style: clean
	@bash scripts/check-style.sh

fix-style: clean
	@bash scripts/check-style.sh -f

