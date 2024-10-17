# Initial process that execs /init.
# This code runs in user space.

#include <common/syscall_v6.h>

# exec(init, argv)
.globl start
start:
        la a0, init
        la a1, argv
        li a7, SYS_v6_exec
        ecall

# for(;;) exit();
exit:
        li a7, SYS_v6_exit
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
