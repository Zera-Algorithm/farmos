# FarmOS

## 文件结构

- `include`/`kern`
    - `dev` 硬件抽象层
    - `fs` 文件系统
    - `lib` 通用库
    - `lock` 锁
    - `mm` 内存管理
    - `proc` 进程管理
    - `trap` 中断处理
- `lib` 通用库
- `linker` 链接脚本
- `scripts` 辅助脚本


## 一些说明
1. Qemu默认加载的OpenSBI位于
    “/usr/local/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin (addresses 0x0000000080000000 - 0x0000000080019b50)”
    所以加载的内核需要避开这段内存区域。
2. 我们约定内核加载的位置为 `0x80200000`. 因为Qemu加载的默认OpenSBI的跳转位置为 `0x80200000`.
    (其实可以设置，见 https://zhuanlan.zhihu.com/p/578765652)
3. 使用OpenSBI作为BIOS启动时，其他核默认是关闭的，需要使用HSM SBI Call来打开。
4. 参考https://zhuanlan.zhihu.com/p/501901665可以实现直接使用vscode的gdb功能来调试内核。
5. 注意：我们使用的用户程序是通过 `riscv64-unknown-elf-gcc` 编译的，所以ELF结构里面的大部分参数都是64位的。

## 参考的仓库列表

* xv6-riscv源码：https://github.com/mit-pdos/xv6-riscv
* 北航MOS操作系统：https://gitee.com/osbuaa/mos
* 2022年一等奖得主"图漏图森破"操作系统：https://gitlab.eduxiji.net/educg-group-13484-858191/19373469-1384

## 未来想做的事情
[ ] 做一个工具，能够自动从.c文件生成其对应的.h文件，并在Makefile中实现

[x] 考虑引入分级日志，能够显示INFO, WARNING, FATAL三种类型的信息

[ ] 实现内核的动态内存分配，可参考Linux的buddy system等

[ ] 更改Makefile的逻辑，支持根据日期决定是否编译。（实现方法：去除一些不必要的 `.PHONY` 项目，如 build 等）

[ ] 扩充内核栈的大小，目前只有一页。

[ ] 解决make不能多线程编译的问题
