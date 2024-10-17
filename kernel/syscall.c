#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include "spinlock.h"
#include "proc.h"
#include <common/syscall_v6.h>
#include "defs.h"

// Fetch the uint64_t at addr from the current process.
int
fetchaddr(uint64_t addr, uint64_t *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64_t) > p->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64_t addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64_t
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64_t *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64_t addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64_t sys_v6_fork(void);
extern uint64_t sys_v6_exit(void);
extern uint64_t sys_v6_wait(void);
extern uint64_t sys_v6_pipe(void);
extern uint64_t sys_v6_read(void);
extern uint64_t sys_v6_kill(void);
extern uint64_t sys_v6_exec(void);
extern uint64_t sys_v6_fstat(void);
extern uint64_t sys_v6_chdir(void);
extern uint64_t sys_v6_dup(void);
extern uint64_t sys_v6_getpid(void);
extern uint64_t sys_v6_sbrk(void);
extern uint64_t sys_v6_sleep(void);
extern uint64_t sys_v6_uptime(void);
extern uint64_t sys_v6_open(void);
extern uint64_t sys_v6_write(void);
extern uint64_t sys_v6_mknod(void);
extern uint64_t sys_v6_unlink(void);
extern uint64_t sys_v6_link(void);
extern uint64_t sys_v6_mkdir(void);
extern uint64_t sys_v6_close(void);
extern uint64_t sys_v6_ioctl(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64_t (*syscalls[])(void) = {
[SYS_v6_fork]    sys_v6_fork,
[SYS_v6_exit]    sys_v6_exit,
[SYS_v6_wait]    sys_v6_wait,
[SYS_v6_pipe]    sys_v6_pipe,
[SYS_v6_read]    sys_v6_read,
[SYS_v6_kill]    sys_v6_kill,
[SYS_v6_exec]    sys_v6_exec,
[SYS_v6_fstat]   sys_v6_fstat,
[SYS_v6_chdir]   sys_v6_chdir,
[SYS_v6_dup]     sys_v6_dup,
[SYS_v6_getpid]  sys_v6_getpid,
[SYS_v6_sbrk]    sys_v6_sbrk,
[SYS_v6_sleep]   sys_v6_sleep,
[SYS_v6_uptime]  sys_v6_uptime,
[SYS_v6_open]    sys_v6_open,
[SYS_v6_write]   sys_v6_write,
[SYS_v6_mknod]   sys_v6_mknod,
[SYS_v6_unlink]  sys_v6_unlink,
[SYS_v6_link]    sys_v6_link,
[SYS_v6_mkdir]   sys_v6_mkdir,
[SYS_v6_close]   sys_v6_close,
[SYS_v6_ioctl]   sys_v6_ioctl,
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
