        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)

        # status = Supervisor, a0 = hartid, a1 = dtb
        la sp, stack0

        li t0, 1024*4
        mv t1, a0
        addi t1, t1, 1
        mul t0, t0, t1
        add sp, sp, t0

        # jump to start() in start.c
        # pass a0 as hartid
        call start
spin:
        j spin
