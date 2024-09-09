/* Physical memory layout
 *
 * qemu -machine virt is set up like this,
 * based on qemu's hw/riscv/virt.c:
 *
 * 00001000 -- boot ROM, provided by qemu
 * 02000000 -- CLINT
 * 0C000000 -- PLIC
 * 10000000 -- uart0
 * 10001000 -- virtio disk
 * KERNBASE -- boot ROM/OpenSBI jumps here in M-mode/S-mode
 *             -kernel loads the kernel here
 *
 * the kernel uses physical memory thus:
 * KERNBASE -- entry.S, then kernel text and data
 * end -- start of kernel page allocation area
 * PHYSTOP -- end RAM used by the kernel
 */

#if defined(__ASSEMBLER__)
# define   U(_x)        (_x)
# define  UL(_x)        (_x)
# define ULL(_x)        (_x)
# define   L(_x)        (_x)
# define  LL(_x)        (_x)
#else
# define  U_(_x)        (_x##U)
# define   U(_x)        U_(_x)
# define  UL(_x)        (_x##UL)
# define ULL(_x)        (_x##ULL)
# define   L(_x)        (_x##L)
# define  LL(_x)        (_x##LL)
#endif

#include "config.h"

/* core local interruptor (CLINT), which contains the timer. */
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) /* cycles since boot. */

/* qemu puts platform-level interrupt controller (PLIC) here. */
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

/* the kernel expects there to be RAM
 * for use by the kernel and user pages
 * from physical address KERNBASE to PHYSTOP.
 */
#define KERNBASE UL(0x80200000)
#define PHYSTOP (KERNBASE + 60*1024*1024)

/* map the trampoline page to the highest address,
 * in both user and kernel space.
 */
#define TRAMPOLINE (MAXVA - PGSIZE)

/* map kernel stacks beneath the trampoline,
 * each surrounded by invalid guard pages.
 */
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

/* User memory layout.
 * Address zero first:
 *   text
 *   original data and bss
 *   fixed-size stack
 *   expandable heap
 *   ...
 *   TRAPFRAME (p->trapframe, used by the trampoline)
 *   TRAMPOLINE (the same page as in the kernel)
 */
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
