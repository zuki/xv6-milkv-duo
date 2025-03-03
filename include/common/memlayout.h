#ifndef INC_MEMLAYOUT_H
#define INC_MEMLAYOUT_H
/* 物理メモリのレイアウト
 *
 * qemu -machine virt は次のように設定されている。
 *   参照: qemuの hw/riscv/virt.c
 *
 * 00001000 -- boot ROM, provided by qemu
 * 02000000 -- CLINT
 * 0C000000 -- PLIC
 * 10000000 -- uart0
 * 10001000 -- virtio disk
 * KERNBASE -- boot ROM/OpenSBI jumps here in M-mode/S-mode
 *             -kernel loads the kernel here
 *
 */

/* cv1800b のペリフェラルは次のように設定されている。
 *
 * 04140000 -- UART0
 * 041C0000 -- UART4
 * 04310000 -- SD0
 * 04320000 -- SD1
 * 04400000 -- boot ROM
 * 05200000 -- SRAM
 * 70000000 -- PLIC
 * 74000000 -- CLINT
 * 80000000 -- DDR
 */

/*
 * カーネルメモリレイアウト:
 * ```
 *  MAXVA         -> ----------------------------
 *  (0x40_0000_0000)
 *                         トランポリン             R-X
 *  0x3F_FFFF_F000 -> ---------------------------
 *                         ガードページ             ---
 *                       カーネルスタック0          RW-
 *                         ガードページ             ---
 *                       カーネルスタック1          RW-
 *                              ....
 *  PHYSTOP       -> ----------------------------
 *  (0x8800_0000)            heap                   RW-
 *  PAGE_START    -> ----------------------------
 *  (0x8060_0000)     カーネル rodata, data, bss    RW-
 *                   ----------------------------
 *                         カーネル text            R-X
 *  KERNBASE      -> ----------------------------
 *  (0x8020_0000)
 *                   ----------------------------
 *                            CLINT                 RW-
 *   0x7400_0000  -> ----------------------------
 *                            PLIC                  RW-
 *   0x7000_0000  -> ----------------------------
 *                             RTC                  RW-
 *   0x0502_6000 -> ----------------------------
 *                             USB                  RW-
 *   0x0434_0000 -> ----------------------------
 *                             SD0                  RW-
 *   0x0431_0000  -> ----------------------------
 *                            UART0                 RW-
 *   0x0414_0000  -> ----------------------------
 *                            RESET                 RW-
 *   0x0300_3000  -> ----------------------------
 *                            CLKGEN                RW-
 *   0x0300_2000  -> ----------------------------
 *                            PINMUX                RW-
 *   0x0300_1000  -> ----------------------------
 *                            SYCTL                 RW-
 *   0x0300_0000  -> ----------------------------
 * ```
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

#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) /* cycles since boot. */

/* qemu puts platform-level interrupt controller (PLIC) here. */
#define PLIC_PRIORITY   (PLIC + 0x0)
#define PLIC_PENDING    (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
/*#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100) */
#define PLIC_SENABLE0(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SENABLE1(hart) (PLIC + 0x2084 + (hart)*0x100)
#define PLIC_SENABLE2(hart) (PLIC + 0x2088 + (hart)*0x100)
#define PLIC_SENABLE3(hart) (PLIC + 0x208c + (hart)*0x100)

#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)

#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

#define PLIC_CTRL (PLIC + 0x1FFFFC)

/* カーネルは物理アドレスKERNBASEからPHYSTOPまで
 * カーネルとユーザのページが使用するRAMがあることを
 * 期待している.
 */
#define KERNBASE UL(0x80200000)
/* PHYSTOP: 0x83e0_0000 */
#define PHYSTOP (KERNBASE + 60*1024*1024)

/* trampolineページはユーザ空間でもカーネル空間でも
 * 最上位のアドレスにマッピングする.
 */
#define TRAMPOLINE (MAXVA - PGSIZE)

/* trampolineの下にカーネルスタックをマッピングする
 * それぞれガードページを挟む.
 */
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

/* カーネルがリンクされるアドレス = KERNELBASE */
#define KERNLINK    (KERNBASE)

/* ユーザメモリレイアウト.
 *  0x40_0000_0000 -> --------------------------- MAXVA (256GB)
 *                         トランポリン             R-X
 *  0x3F_FFFF_F000 -> --------------------------- TRAMPOLINE
 *                         トラップフレーム
 *  0x3F_FFFF_E000 -> --------------------------- TRAPFRAME
 *                          ガードページ
 *  0x3F_FFFF_D000 -> --------------------------- USERTOP (STACKTOP/MMAPTOP)
 *
 *
 *                         mmap領域
 *  0x20_0000_0000 -> --------------------------- MMAPBASE (128GB)
 *
 *                         ヒープ領域
 *                    --------------------------- p->sz = stack_top
 *                        スタック (4KB)
 *                    --------------------------- p->sp
 *                       ガードページ (4KB)
 *                    ---------------------------
 *                         コード領域
 *  0x00_0000_0000 -> ---------------------------
 */

#define TRAPFRAME   (TRAMPOLINE - PGSIZE)

#define USERTOP     (TRAPFRAME - PGSIZE) /* ユーザ空間の最上位アドレス (ガードページをはさむ) */
#define STACKTOP    USERTOP             /* スタックはUSERTOPから */
#define MMAPBASE    UL(0x2000000000)    /* mmapアドレスの基底アドレス */
#define MMAPTOP     USERTOP             /* ARGV, ENVPなどはstack_topからmmapするため */

#endif
