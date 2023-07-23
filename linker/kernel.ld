OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /*
   * ensure that entry.S / _entry is at 0x80000000,
   * where qemu's -kernel jumps.
   */
  . = 0x80200000;

  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    /* 需要定义一个trampsec: .section trampsec。
      此section是用户态下的中断入口程序，与内核共享，
      需要单独占据一个页的长度(参考trampoline.S)
     */
    _trampoline = .;
    *(trampSec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");

	/* 用户信号trampoline，有PTE_U权限 */
	_sigSec = .;
	*(sigSec)
	. = ALIGN(0x1000);
	ASSERT(. - _sigSec == 0x1000, "error: sigSec larger than one page");

    . = ALIGN(0x1000);
    PROVIDE(end_text = .);
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

  .initcall_fs : {
    . = ALIGN(8);
    __initcall_fs_start = .;
    KEEP(*(.initcall_fs))  /* 避免此段被链接器垃圾清理（因为这些符号在代码中没有实际使用到） */
	__initcall_fs_end = .;
  }

  . = ALIGN(0x1000);
  PROVIDE(end = .); /* PROVIDE的作用是，如果C代码中定义了该符号，那么就使用C中的符号 */
}
