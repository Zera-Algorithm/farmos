## Trap与中断

### 概述

FarmOS 的 Trap 分为内核 Trap 和用户 Trap，采用不同的Trap处理程序。

相关文件的目录结构如下：

```txt
kern
└── trap
    ├── kernelvec.S
    ├── syscall.c
    ├── trampoline.S
    ├── trap.c
	└── Makefile

include
└── trap
    ├── syscall_ids.h
    └── trap.h
```

### 启动过程

在启动阶段，首先初始化各hart的中断处理向量，之后使用SBI初始化核内时钟，之后初始化中断控制器，最后为每个核配置中断控制器。

* `main` 函数
  * `trapInitHart`：初始化各个核的中断处理向量
  * `timerInit`：初始化核内时钟
  * `plicInit`：初始化PLIC中断控制器，配置接收 VIRTIO0 的IRQ中断
  * `plicInitHart`：配置各个核的中断控制器，配置当前核可以接收 VIRTIO0 的IRQ中断

### 中断获取与处理

`kern/dev/dev.c`

- `plicInit`：初始化PLIC中断控制器，配置接收 VIRTIO0 的IRQ中断

- `plicInitHart`：配置各个核的中断控制器，配置当前核可以接收 VIRTIO0 的IRQ中断
- `plicClaim`：向PLIC索要当前中断的IRQ编号
- `plicComplete`：告知plic我们已经处理完了irq代指的中断

`kern/dev/timer.c`

* `timerInit`: 打开全局中断，设置核内时钟下一Tick的时间，以初始化时钟
* `timerSetNextTick`: 在发生时钟中断时设置下一个时钟Tick的时间

### 内核Trap处理

内核Trap处理的入口函数为 `kernelvec.S: kernelvec`。其主要工作是将当前的上下文存储在内核栈上，然后跳转到C语言Trap处理函数 `kerneltrap`。

`kerneltrap`: (`trap.c`) 针对中断和异常两种情况做处理：

* 中断：对于时钟中断，设置其下一个tick的时间；对于外部中断，使用PLIC中断控制器获取发生中断的IRQ编号，根据编号跳转到对应的中断处理函数做处理，处理完使用`plicComplete`告知PLIC中断已处理。
* 异常：打印异常的上下文信息、异常原因、异常发生的内存地址等，帮助诊断内核bug。

### 用户态Trap处理

用户态Trap处理的入口函数为 `trampoline.S: userVec`。该函数位于trampoline（即用户态与内核态的跳板位置，因为此位置在用户态和内核态的虚拟内存都映射到同一块物理内存），主要作用为：

* 保存上下文信息（包括寄存器信息、EPC等）到进程的Trapframe页
* 加载内核需要的信息，如处理器核号tp，内核sp等
* **加载内核页表**
* 跳转到内核用户态Trap处理函数 `userTrap`

用户态处理函数 `userTrap`（`trap.c`）主要处理以下情形：

* 中断：目前仅处理时钟中断，设置下一tick
* **无写权限位时**的页写入异常：跳转到处理函数，实现写时复制
* **无页表映射时**的页读取、写入、指令执行异常：跳转到处理函数，实现根据program Break的被动内存分配
* 系统调用：跳转到对应的系统调用函数
* 其他异常：打印异常时的上下文信息，帮助定位bug

处理完上述Trap后，除最后一种panic外，调用 `userTrapReturn` 返回用户态。其中，`userTrapReturn(trap.c)` 的代码如下：

```c
/**
 * @brief 从内核态返回某个用户的用户态
 */
void userTrapReturn() {
	// log(DEFAULT, "userTrap return begins:\n");
	/**
	 * @brief 获取当前CPU上应当运行的下一个进程
	 * @note 若发生进程切换，需要更改CPU上的下一个进程，以在这里切换
	 */
	struct Proc *p = myProc();

	// 关中断，避免中断对S态到U态转换的干扰
	intr_off();

	// 加载trapoline地址
	u64 trampolineUserVec = TRAMPOLINE + (userVec - trampoline);
	w_stvec(trampolineUserVec);

	/**
	 * @brief 通过trapframe设置与用户态共享的一些数据，以便在trampoline中使用
	 * @note 包括：页目录satp、trap handler、hartid
	 */
	p->trapframe->kernel_satp = r_satp();
	p->trapframe->trap_handler = (u64)userTrap;
	p->trapframe->hartid = cpuid();

	extern char stack0[];
	p->trapframe->kernel_sp = (u64)stack0 + PAGE_SIZE * (cpuid() + 1);

	// 设置S态Previous Mode and Interrupt Enable，
	// 以在sret时恢复状态
	u64 sstatus = r_sstatus();
	sstatus &= ~SSTATUS_SPP; // SPP = 0: 用户状态
	sstatus |= SSTATUS_SPIE; // SPIE = 1: 开中断
	w_sstatus(sstatus);

	// 恢复epc
	w_sepc(p->trapframe->epc);

	u64 satp = MAKE_SATP(p->pageTable);

	u64 trampolineUserRet = TRAMPOLINE + (userRet - trampoline);
	// log(DEFAULT, "goto trampoline, func = 0x%016lx\n", trampolineUserRet);
	((void (*)(u64))trampolineUserRet)(satp);
}
```

