# Initial process that execs /init.
# This code runs in user space.

# execve(init, argv, envp)
.globl start
start:
        la a0, init
        la a1, argv
        li a2, 0
        li a7, 221
        ecall

# for(;;) exit();
exit:
        li a7, 93
        ecall
        jal exit

# char init[] = "/init\0"; 0x2f696e697400
init:
  .string "/init"

# char tz[] = "TZ=JST-9\0"; 0x545a3d4a53542d3900
#tz:
#  .string "TZ=JST-9"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .dword init, 0

# char *envp[] = { tz, 0 };
#.p2align 2
#envp:
#  .dword tz, 0
