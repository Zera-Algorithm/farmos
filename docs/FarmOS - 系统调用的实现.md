## 系统调用的处理

根据区域赛系统调用的说明，系统调用方式遵循RISC-V ABI,所有参数全部存储在栈上，即调用号存放在a7寄存器中,6个参数分别储存在a0-a5寄存器中,返回值保存在a0中。

### 系统调用入口

`syscall.c: syscallEntry()` 是系统调用入口，代码如下所示：

```c
/**
 * @brief 系统调用入口。会按照tf中传的参数信息（a0~a7）调用相应的系统调用函数，并将返回值保存在a0中
 *
 */
void syscallEntry(Trapframe *tf) {
	// S态时间审计
	u64 startTime = getTime();

	u64 sysno = tf->a7;
	// 系统调用最多6个参数
	u64 (*func)(u64, u64, u64, u64, u64, u64);

	// 根据反汇编结果，一个ecall占用4字节的空间
	tf->epc += 4;

	// 获取系统调用函数
	func = (u64(*)(u64, u64, u64, u64, u64, u64))syscallTable[sysno];
	if (func == 0) {
		tf->a0 = SYSCALL_ERROR;
		warn("unimplemented or unknown syscall: %d\n", sysno);
		return;
	}

	// 将系统调用返回值放入a0寄存器
	tf->a0 = func(tf->a0, tf->a1, tf->a2, tf->a3, tf->a4, tf->a5);

	// S态时间审计
	u64 endTime = getTime();
	myProc()->procTime.totalStime += (endTime - startTime);
}
```

其主要工作为从 `a7` 寄存器获取系统调用号，将用户指令后移4位（EPC指令的长度为4Byte），然后查询系统调用表。如果存在对应系统调用号的系统调用服务函数，就用传入的 `a0~a5` 共6个寄存器作为参数调用该函数并将返回值存储在 `a0` 中；否则报错并直接返回。

关于各个系统调用的实现，可阅读 [区域赛官方系统调用手册](./%E5%8C%BA%E5%9F%9F%E8%B5%9B%E5%AE%98%E6%96%B9%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8%E6%89%8B%E5%86%8C.md)，在此不再赘述。

### `GetTimeOfDay` 系统调用

对于 `getTimeOfDay(u64 pTimeSpec)` 系统调用，观察到许多队伍没有完全实现该功能，大部分是使用了qemu开机的时间来代替。

我们调查了 [Qemu Virt](https://www.qemu.org/docs/master/system/riscv/virt.html?highlight=virt%20riscv) 机器上的设备清单，发现其板载一个Google Goldfish RTC，可以用于获得当前的时间，以及实现板级的定时器功能等。我们搜索了github，找到了该设备的 [官方驱动](https://github.com/RMerl/asuswrt-merlin.ng/blob/f4e3563d403dff0ccf610f20504f4a3a0580f5e1/release/src-rt-5.04axhnd.675x/kernel/linux-4.19/drivers/rtc/rtc-goldfish.c#L1) ，经过简单地整理实现了FarmOS的RTC驱动。

该驱动实现了一个 `rtcReadTime()` 函数，能够获取以us计的Unix时间戳。
