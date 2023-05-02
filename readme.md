一些说明：
1. Qemu默认加载的OpenSBI位于
    “/usr/local/bin/../share/qemu/opensbi-riscv64-generic-fw_dynamic.bin (addresses 0x0000000080000000 - 0x0000000080019b50)”
    所以加载的内核需要避开这段内存区域。
2. 我们约定内核加载的位置为 `0x80200000`. 因为Qemu加载的默认OpenSBI的跳转位置为 `0x80200000`.
    (其实可以设置，见 https://zhuanlan.zhihu.com/p/578765652)
3. 使用OpenSBI作为BIOS启动时，其他核默认是关闭的，需要使用HSM SBI Call来打开。