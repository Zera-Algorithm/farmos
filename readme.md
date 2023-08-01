# FarmOS

## FarmOS 简介
FarmOS是北京航空航天大学的三名本科生共同开发的基于RISC-V的宏内核类Unix操作系统，现已支持区域赛阶段的所有系统调用。

## 快速开始

### 环境准备

* 安装基础构建工具 GNU/make、GNU/Bash
* 安装 Python3
* 安装 RISCV 64位 gcc 编译工具链
	* riscv64-unknown-elf-gcc
	* riscv64-unknown-elf-ld
	* ...
	* riscv64-unknown-elf-gdb
* 安装 Qemu-7.0.0 for Riscv64

在 Ubuntu22.04 下安装 riscv64-unknown-elf-* 系列编译器：

```
sudo apt install gcc-riscv64-unknown-elf
```

调试器则需要自行编译安装。

安装Qemu：

```
sudo apt install qemu-system-riscv64
```

### 编译FarmOS

克隆本仓库代码到本地，然后运行：

```
make
```

即可编译得到 FarmOS 内核二进制文件 `kernel-qemu`.

### 运行FarmOS

运行：

```
make qemu
```

可以运行FarmOS操作系统。

### 调试FarmOS

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


### 第一阶段文档

* []

## 一些说明
1. Qemu默认加载的OpenSBI位于
    `/usr/local/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin` (对于128MB内存，其位置位于 `0x0000000080000000` - `0x0000000080019b50`)”
    所以加载的内核需要避开这段内存区域。
2. 我们约定内核加载的位置为 `0x80200000`. 因为Qemu加载的默认OpenSBI的跳转位置为 `0x80200000`.
    (其实可以设置，见 https://zhuanlan.zhihu.com/p/578765652)
3. 使用OpenSBI作为BIOS启动时，其他核默认是关闭的，需要使用HSM SBI Call来打开。
4. 参考 https://zhuanlan.zhihu.com/p/501901665 可以实现直接使用vscode的gdb功能来调试内核。
5. 注意：我们使用的用户程序是通过 `riscv64-unknown-elf-gcc` 编译的，所以加载的ELF结构里面的大部分参数都是64位的。

## 参考的仓库列表

* xv6-riscv源码：https://github.com/mit-pdos/xv6-riscv
* 北航MOS操作系统：https://gitee.com/osbuaa/mos
* 2022年一等奖得主"图漏图森破"操作系统：https://gitlab.eduxiji.net/educg-group-13484-858191/19373469-1384

## 未来想做的事情
<!-- 打钩： -->
<!-- [&#10004;] -->
[ ] 做一个工具，能够自动从.c文件生成其对应的.h文件，并在Makefile中实现

[&#10004;] 引入分级日志，能够显示INFO, WARNING, FATAL三种类型的信息

[ ] 实现内核的动态内存分配，可参考Linux的buddy systemh和slab等

[ ] 更改Makefile的逻辑，将 `.c` 文件引入的 `.h` 加入到 `.c` 文件的依赖中，从而支持根据日期决定是否编译。教训：之前没有写 `.c` 到 `.h` 的依赖，结果 `.h` 中的定义改变之后 `.c` 编译成的 `.o` 文件并没有改变，导致出问题。

[ ] 检测内核栈溢出是否会发出告警，之后设置一个尽量大的内核栈

[ ] 解决make不能多线程编译的问题

[ ] Syscall的Profiling

[ ] 发送SIGKILL信号后，将阻塞在IO上的syscall唤醒，继续完成接下来的事务，待syscall结束时才会最终kill。
syscall要保证自己只会进入一个阻塞IO，即被唤醒后就能立刻返回用户空间。现在一般的睡眠过程是睡眠等待某个条件成立，
比如管道读取是等待管道不为空或者管道关闭这二者之一成立。那么由SIGKILL完成的唤醒肯定不会满足这个条件，需要加一个额外的条件(td->td_killed)，帮助其顺利完成**拖尾**的syscall返回用户空间。TODO：之后需要检查每一个IO等待的睡眠是否存在这个问题。
