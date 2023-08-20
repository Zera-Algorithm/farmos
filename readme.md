# FarmOS

FarmOS 是北京航空航天大学的三名本科生共同开发的基于 RISC-V 的宏内核类 Unix 操作系统。

## 快速开始

### 环境准备

* 安装基础构建工具 GNU/make、GNU/Bash
* 安装 Python 3
* 安装 RISCV 64位 gcc 编译工具链
	* `riscv64-unknown-elf-gcc`
	* `riscv64-unknown-elf-ld`
	* ...
	* `riscv64-unknown-elf-gdb`
* 安装 Qemu-7.0.0 for Riscv64

在 Ubuntu 22.04 下安装 `riscv64-unknown-elf-*` 系列编译器：

```
sudo apt install gcc-riscv64-unknown-elf
```

调试器则需要自行编译安装。

安装 Qemu：

```
sudo apt install qemu-system-riscv64
```

### 编译 FarmOS

克隆本仓库代码到本地，然后运行：

```
make
```

即可编译得到 FarmOS 内核二进制文件 `kernel-qemu`.

### 运行 FarmOS

运行：

```
make qemu
```

可以运行FarmOS操作系统。

### 调试 FarmOS

运行：
```
make qemu-gdb
```

之后新建一个终端窗口，输入：
```
riscv64-unknown-elf-gdb kernel-qemu
```
用于加载内核二进制文件中的符号并开始调试。

然后在gdb提示符下，输入 `target remote localhost:26000` 连接调试端口。这样就可以开始调试了。

## 目录结构

- `include`/`kern`
	- `boot` 启动相关代码
    - `dev` 硬件抽象层
	- `driver` 驱动
    - `fs` 文件系统
    - `lib` 通用库
    - `lock` 锁
    - `mm` 内存管理
    - `proc` 进程管理
    - `trap` 中断处理
	- `kernel.asm` 内核反汇编文件
	- `Makefile`
- `lib` 用户与内核的通用库
- `linker` 链接脚本
- `scripts` 辅助脚本
- `user` 用户代码


## 常用的 Makefile 命令

* `make` / `make all`：生成内核镜像文件
* `make clean`：清空编译中间文件和目标文件
* `make qemu`：在qemu中运行FarmOS
* `make qemu-gdb`：使用gdb调试内核
* `make check-style`：使用clang-format检查C代码格式是否符合规范。代码规范位于 `.clang-format` 文件内
* `make fix-style`：使用clang-format自动修复C代码的格式

## 文档列表

### 架构与工具使用笔记

* [gdb调试方法](./docs/gdb%E8%B0%83%E8%AF%95%E6%96%B9%E6%B3%95.md)
* [RISCV64寄存器](./docs/RISCV64%E5%AF%84%E5%AD%98%E5%99%A8.md)
* [SBI中文介绍(译)](./docs/SBI%EF%BC%9ASupervisor%20Software%20Binary%20Interface%20%E8%BD%AF%E4%BB%B6%E4%BA%8C%E8%BF%9B%E5%88%B6%E6%8E%A5%E5%8F%A3%EF%BC%88%E8%AF%91%EF%BC%89.md)

### 区域赛文档

* [引导](./docs/FarmOS%20-%20boot.md)
* [Trap和中断](./docs/FarmOS%20-%20Trap%E4%B8%8E%E6%97%B6%E9%92%9F%E4%B8%AD%E6%96%AD.md)
* [内存管理](./docs/FarmOS%20-%20%E5%86%85%E5%AD%98%E7%AE%A1%E7%90%86.md)
* [用户进程与调度](./docs/FarmOS%20-%20%E7%94%A8%E6%88%B7%E8%BF%9B%E7%A8%8B%E4%B8%8E%E8%B0%83%E5%BA%A6.md)
* [系统调用的处理](./docs/FarmOS%20-%20%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8%E7%9A%84%E5%AE%9E%E7%8E%B0.md)
* [VFS](./docs/FarmOS%20-%20VFS.md)

### 全国赛第一阶段文档

说明：我们国赛的第一阶段qemu赛道的提交放置在本仓库的archive/final1-qemu分支；hifive unmatched开发板赛道的提交放置在本仓库的archive/final2-unmatced分支。

* [文件系统](./docs/第一阶段-文件系统.md)
* [sd卡驱动](./docs/FarmOS%20-%20SD%20卡驱动.md)
* [Socket](./docs/FarmOS%20-%20Socket.md)
* [内存管理](./docs/FarmOS%20-%20内存管理.md)
* [进程与线程](./docs/FarmOS%20-%20进程与线程.md)
* [多核同步互斥机制设计](./docs/FarmOS%20-%20多核同步互斥机制.md)

### 全国赛第二阶段文档

[sshd适配记录](./docs/现场赛-sshd适配记录.md)


## 参考资料

- [XV6(RISC-V)](https://github.com/mit-pdos/xv6-riscv)
- [MOS - 北京航空航天大学操作系统课程实验](https://gitee.com/osbuaa/mos)
- [图漏图森破 - 2022 年参赛作品](https://gitlab.eduxiji.net/educg-group-13484-858191/19373469-1384)


