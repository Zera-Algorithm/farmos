# 配置一些常用的编译工具及其参数

QEMU		= qemu-system-riscv64
TOOLPREFIX	= riscv64-unknown-elf-
PYTHON		= python3

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# 编译C语言时的参数
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
# CFLAGS = -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
# 增加外部可控宏定义
CFLAGS += -DNCPU=2

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

# -bios default: 缺省的SBI实现(OpenSBI)
QEMUOPTS = -machine virt -bios default -kernel $(KERNEL_ELF) -m 128M -smp $(NCPU) -nographic
# 比赛要求virtio legacy驱动
# QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
# 加载的是一个virtio块设备
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
