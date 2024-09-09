        # qemu -kernel loads the kernel at KERNBASE
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at KERNBASE.
.section .text
.global _start
_start:
	j _entry
	.word 0
	.balign 8
	.dword 0x200000
	.dword _end - _start
	.dword 0
	.word 0
	.word 0
	.dword 0
	.ascii "RISCV\0\0\0"
	.balign 4
	.ascii "RSC\x05"
	.word 0
.global _entry
_entry:
        # OpenSBI sets up the following registers:
        #   a0 = hartid
        #   a1 = pointer to fdt
        #
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        la sp, stack0
#ifdef CONFIG_RISCV_M_MODE
        li a0, 1024*4
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
#else
        li a2, 1024*4
        addi a3, a0, 1
        mul a2, a2, a3
        add sp, sp, a2
#endif
        # jump to start() in start.c
        call start
spin:
        j spin
