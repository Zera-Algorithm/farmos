# 配置一些常用的编译工具及其参数

QEMU		= qemu-system-riscv64
TOOLPREFIX	= riscv64-unknown-elf-
PYTHON		= python3
TEST_RUNNER = ./official_tests/user/src/oscomp/test_runner.py
OS_OUTPUT	= os_output.txt
OUTPUT_JSON = output.json

ifndef NCPU
NCPU := 2
endif

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# 编译C语言时的参数
CFLAGS = -Wall -Werror -fno-omit-frame-pointer -gdwarf-2
# CFLAGS = -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
# -M 生成一个.D文件，记录.c文件的头文件依赖关系
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
# 增加外部可控宏定义
CFLAGS += -DNCPU=$(NCPU)

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

RELEASE_CFLAGS   := $(CFLAGS) -O2
RELEASE_LDFLAGS  := $(LDFLAGS) -O --gc-sections
DEBUG_CFLAGS     := $(CFLAGS) -O -g -ggdb

# 设为release模式，会使用O2优化
# 设为debug模式，开O1
ifeq ($(FARMOS_PROFILE),debug)
	CFLAGS   := $(DEBUG_CFLAGS)
else
	CFLAGS   := $(RELEASE_CFLAGS)
	LDFLAGS  := $(RELEASE_LDFLAGS)
endif

# 子文件夹的Makefile所公用的功能，引入包含头文件依赖的.d文件
DEPS = $(wildcard *.d)
-include $(DEPS)
