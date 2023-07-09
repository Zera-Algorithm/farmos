# 内核和用户程序的存放目录
KERN = kern
USER = user
LIB  = lib
KERNEL_LD := linker/kernel.ld
INCLUDES := -I./include
KERNEL_ELF := kernel-qemu
# kernel的二进制文件
KERNEL_BIN := hifive.bin

include include.mk

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# 编译好待链接的目标文件
# 这里必须改，不然入口_entry不在0x80200000
OBJS := $(KERN)/*/*.o $(LIB)/*.o $(USER)/*.x $(KERN)/*/*/*.o
modules := $(KERN) $(LIB) $(USER)

.PHONY: all clean $(modules)

all: $(KERNEL_ELF)

# 生成 kernel，并将其反汇编到kernel.asm
$(KERNEL_ELF): $(modules) $(KERNEL_LD)
	$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $(KERNEL_ELF) $(OBJS)
	$(OBJDUMP) -S $(KERNEL_ELF) > $(KERN)/kernel.asm

# modules即为 kern 各目录的生成产物，一般是一些 .o 文件
$(modules):
	$(MAKE) build --directory=$@

# TODO: 需要实现。现在仅仅是使用了一个使用mkfs创建的默认镜像
fs.img:
	cp backup_fs.img fs.img
	# dd if=/dev/zero of=fs.img bs=16k count=1024
	# mkfs.vfat -F 12 fs.img

hifive: $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

# 可以暂时不需要镜像文件
# qemu: $(KERNEL_ELF)
qemu: $(KERNEL_ELF) fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

# 可以暂时不需要镜像文件
# qemu-gdb: $(KERNEL_ELF) .gdbinit
qemu-gdb: $(KERNEL_ELF) .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

comp: all fs.img
	qemu-system-riscv64 -machine virt -kernel $(KERNEL_ELF) -m 128M -nographic -smp $(NCPU) -bios default -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -no-reboot > $(OS_OUTPUT)

judge: $(OS_OUTPUT)
	$(PYTHON) $(TEST_RUNNER) $(OS_OUTPUT) > $(OUTPUT_JSON)

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

real-test: clean all comp judge
