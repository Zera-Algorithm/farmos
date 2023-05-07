## riscv-xv6启动流程：

1. `kernel.ld` 指示加载内核到`0x80000000`。这个是qemu的-kernel参数要跳转到的地方。

   ```linker script
   OUTPUT_ARCH( "riscv" )
   ENTRY( _entry )
   
   SECTIONS
   {
     /*
      * ensure that entry.S / _entry is at 0x80000000,
      * where qemu's -kernel jumps.
      */
     . = 0x80000000;
   
     .text : {
       *(.text .text.*)
       . = ALIGN(0x1000);
       _trampoline = .;
       *(trampsec)
       . = ALIGN(0x1000);
       ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
       PROVIDE(etext = .);
     }
   
     .rodata : {
       . = ALIGN(16);
       *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
       . = ALIGN(16);
       *(.rodata .rodata.*)
     }
   
     .data : {
       . = ALIGN(16);
       *(.sdata .sdata.*) /* do not need to distinguish this from .data */
       . = ALIGN(16);
       *(.data .data.*)
     }
   
     .bss : {
       . = ALIGN(16);
       *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
       . = ALIGN(16);
       *(.bss .bss.*)
     }
   
     PROVIDE(end = .);
   }
   ```

2. 入口函数位于 `entry.S`。用于为每一个Hart分配栈空间(位于stack0)，然后跳转到start函数。

   ```assembly
   .section .text
   .global _entry
   _entry:
           # set up a stack for C.
           # stack0 is declared in start.c,
           # with a 4096-byte stack per CPU.
           # sp = stack0 + (hartid * 4096)
           la sp, stack0
           li a0, 1024*4
           la t0, dtb_entry # 读取dtb_entry并保存
           sd a1, 0(t0)
           csrr a1, mhartid
           addi a1, a1, 1
           mul a0, a0, a1
           add sp, sp, a0
           # jump to start() in start.c
           call start
   spin:
           j spin
   ```

3. `start.c` 设置 MSTATUS 寄存器，下一步跳转到Supervisor态；设置EPC，控制下一步跳转到main；关闭分页机制；将所有的**中断**和**异常**都**委托**给Supervisor态；将所有物理内存的访问权限都分配给Supervisor态；启动时钟中断(`timerinit`, )；将当前的hard id都保存在tp寄存器（Thread Pointer）；之后使用`mret`指令跳转到Supervisor态。

   ```c
   // entry.S jumps here in machine mode on stack0.
   void
   start()
   {
     // set M Previous Privilege mode to Supervisor, for mret.
     unsigned long x = r_mstatus();
     x &= ~MSTATUS_MPP_MASK;
     x |= MSTATUS_MPP_S;
     w_mstatus(x);
   
     // set M Exception Program Counter to main, for mret.
     // requires gcc -mcmodel=medany
     w_mepc((uint64)main);
   
     // disable paging for now.
     w_satp(0);
   
     // delegate all interrupts and exceptions to supervisor mode.
     w_medeleg(0xffff);
     w_mideleg(0xffff);
     w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
   
     // configure Physical Memory Protection to give supervisor mode
     // access to all of physical memory.
     w_pmpaddr0(0x3fffffffffffffull);
     w_pmpcfg0(0xf);
   
     // ask for clock interrupts.
     timerinit();
   
     // keep each CPU's hartid in its tp register, for cpuid().
     int id = r_mhartid();
     w_tp(id);
   
     // switch to supervisor mode and jump to main().
     asm volatile("mret");
   }
   ```

   `timerinit`，此中断由M状态捕获，并转交给S态。

   详细：用户态发生时钟中断时，首先由M状态捕获，进入timervec，然后设置S态的software int中断，mret返回用户态；之后用户态检测到S态的software异常，跳转到S态，执行对应的中断处理程序，最后通过sret返回用户态。

   ```asm
   timervec:
           # start.c has set up the memory that mscratch points to:
           # scratch[0,8,16] : register save area.
           # scratch[24] : address of CLINT's MTIMECMP register.
           # scratch[32] : desired interval between interrupts.
           
           csrrw a0, mscratch, a0
           sd a1, 0(a0)
           sd a2, 8(a0)
           sd a3, 16(a0)
   
           # schedule the next timer interrupt
           # by adding interval to mtimecmp.
           ld a1, 24(a0) # CLINT_MTIMECMP(hart)
           ld a2, 32(a0) # interval
           ld a3, 0(a1)
           add a3, a3, a2
           sd a3, 0(a1)
   
           # arrange for a supervisor software interrupt
           # after this handler returns.
           li a1, 2
           csrw sip, a1
   
           ld a3, 16(a0)
           ld a2, 8(a0)
           ld a1, 0(a0)
           csrrw a0, mscratch, a0
   
           mret
   ```

   kernelvec:

   ```asm
   kernelvec:
           # make room to save registers.
           addi sp, sp, -256
   
           # save the registers.
           ...
   
           # call the C trap handler in trap.c
           call kerneltrap
   
           # restore registers.
           ...
   
           addi sp, sp, 256
   
           # return to whatever we were doing in the kernel.
           sret
   ```

4. `main` 函数：执行一系列init，之后运行调度器。