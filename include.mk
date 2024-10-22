# 配置一些常用的编译工具及其参数
QEMU		= qemu-system-riscv64
PYTHON		= python3
TEST_RUNNER = ./official_tests/user/src/oscomp/test_runner.py
OS_OUTPUT	= os_output.txt
OUTPUT_JSON = output.json

# Risc-V 工具链
TOOLPREFIX	= riscv64-unknown-elf-
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump


# 配置默认MACHINE(virt/sifive_u)
ifndef MACHINE
MACHINE := virt
endif

# 配置默认模式(qemu/board)
ifndef MODE
MODE := qemu
endif

# 编译 C 语言时的参数
CFLAGS = -Wall -Werror -fno-omit-frame-pointer -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -DNCPU=$(NCPU)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

# 系统编译时参数
ifneq ($(MODE), board)
CFLAGS += -DQEMU
NCPU := 3
else
NCPU := 5
endif

# 机器类型：
ifeq ($(MACHINE), sifive_u)
CFLAGS += -DSIFIVE
else
CFLAGS += -DVIRT
endif


# 链接时的参数
LDFLAGS = -z max-page-size=4096

RELEASE_CFLAGS   := $(CFLAGS) -O3
RELEASE_LDFLAGS  := $(LDFLAGS) -O --gc-sections
DEBUG_CFLAGS     := $(CFLAGS) -O0 -g -ggdb

# 优化模式：
# 设为release模式，会使用O2优化
# 设为debug模式，开O1
ifeq ($(FARMOS_PROFILE),debug)
	CFLAGS   := $(DEBUG_CFLAGS)
else
	CFLAGS   := $(RELEASE_CFLAGS)
	LDFLAGS  := $(RELEASE_LDFLAGS)
endif

# QEMU 启动参数
QEMUOPTS = -machine $(MACHINE) -bios default -kernel $(KERNEL_ELF) -m 1G -smp $(NCPU) -nographic

ifeq ($(MACHINE),virt)
	QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
	QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
	QEMUOPTS += -device virtio-net-device
else ifeq ($(MACHINE),sifive_u)
	QEMUOPTS += -drive file=fs.img,if=sd,format=raw
endif

# 子文件夹的Makefile所公用的功能，引入包含头文件依赖的.d文件
DEPS = $(wildcard *.d)
-include $(DEPS)
