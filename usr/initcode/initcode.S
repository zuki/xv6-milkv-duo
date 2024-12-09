# Initial process that execs /init.
# This code runs in user space.

# exec(init, argv)
.globl start
start:
        la a0, init
        la a1, argv
        li a7, 221
        ecall

# for(;;) exit();
exit:
        li a7, 93
        ecall
        jal exit

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .long init
  .long 0
