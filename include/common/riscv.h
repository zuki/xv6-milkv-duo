#ifndef INC_RISCV_H
#define INC_RISCV_H

#ifndef __ASSEMBLER__

#include <common/types.h>

// これはどのhart (core) か?
static inline uint64_t
r_mhartid()
{
  uint64_t x;
  asm volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// mstatus: マシンステータスレジスタ

#define MSTATUS_MPP_MASK (3L << 11) // 以前のモード.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)       // マシンモードの割り込みを有効にする

static inline uint64_t
r_mstatus()
{
  uint64_t x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void
w_mstatus(uint64_t x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

// mepc: machine exception program counter
// 例外から復帰した際に実行する命令アドレスを保持する
// (例外の生じた目入れの次のアドレス）
static inline void
w_mepc(uint64_t x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// sstatus: スーパーバイザステータスレジスタ

#define SSTATUS_SPP (1L << 8)  // 以前のモード, 1=スーパーバイザ, 0=ユーザ
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static inline uint64_t
r_sstatus()
{
    uint64_t x;
    asm volatile("csrr %0, sstatus" : "=r" (x) );
    return x;
}

static inline void
w_sstatus(uint64_t x)
{
    asm volatile("csrw sstatus, %0" : : "r" (x));
}

// sip: スーパーバイザ割り込み保留レジスタ
static inline uint64_t
r_sip()
{
    uint64_t x;
    asm volatile("csrr %0, sip" : "=r" (x) );
    return x;
}

static inline void
w_sip(uint64_t x)
{
    asm volatile("csrw sip, %0" : : "r" (x));
}

// sie: スーパーバイザ割り込みテーブル
#define SIE_SEIE (1L << 9) // 外部割り込み
#define SIE_STIE (1L << 5) // タイマー割り込み
#define SIE_SSIE (1L << 1) // ソフトウェア割り込み

static inline uint64_t
r_sie()
{
    uint64_t x;
    asm volatile("csrr %0, sie" : "=r" (x) );
    return x;
}

static inline void
w_sie(uint64_t x)
{
    asm volatile("csrw sie, %0" : : "r" (x));
}

// mie: マシンモード割り込みテーブル
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software

static inline uint64_t
r_mie()
{
    uint64_t x;
    asm volatile("csrr %0, mie" : "=r" (x) );
    return x;
}

static inline void
w_mie(uint64_t x)
{
    asm volatile("csrw mie, %0" : : "r" (x));
}

// sepc: supervisor exception program counter
// 例外から復帰した際に実行する命令アドレスを保持する
static inline void
w_sepc(uint64_t x)
{
    asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64_t
r_sepc()
{
    uint64_t x;
    asm volatile("csrr %0, sepc" : "=r" (x) );
    return x;
}

// medeleg: マシン例外委譲レジスタ
static inline uint64_t
r_medeleg()
{
    uint64_t x;
    asm volatile("csrr %0, medeleg" : "=r" (x) );
    return x;
}

static inline void
w_medeleg(uint64_t x)
{
    asm volatile("csrw medeleg, %0" : : "r" (x));
}

// mideleg: マシン割り込み委譲レジスタ
static inline uint64_t
r_mideleg()
{
    uint64_t x;
    asm volatile("csrr %0, mideleg" : "=r" (x) );
    return x;
}

static inline void
w_mideleg(uint64_t x)
{
    asm volatile("csrw mideleg, %0" : : "r" (x));
}

// stvec: スーパーバイザトラップベクタ基底アドレス
// 低位2ビットはモード
static inline void
w_stvec(uint64_t x)
{
    asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64_t
r_stvec()
{
    uint64_t x;
    asm volatile("csrr %0, stvec" : "=r" (x) );
    return x;
}

// mtvec: マシンモード割り込みベクタ
static inline void
w_mtvec(uint64_t x)
{
    asm volatile("csrw mtvec, %0" : : "r" (x));
}

// 物理メモリ保護レジスタ
static inline void
w_pmpcfg0(uint64_t x)
{
    asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static inline void
w_pmpaddr0(uint64_t x)
{
    asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

// riscvのsv39ページテーブルスキームを使用する (Bit[63:60] = 8).
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64_t)pagetable) >> 12))

// SATP (supervisor address translation and protection)レジスタ
// ページテーブルのアドレスを保持する
static inline void
w_satp(uint64_t x)
{
    asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64_t
r_satp()
{
    uint64_t x;
    asm volatile("csrr %0, satp" : "=r" (x) );
    return x;
}

static inline void
w_mscratch(uint64_t x)
{
    asm volatile("csrw mscratch, %0" : : "r" (x));
}

static inline uint64_t
r_sscratch()
{
    uint64_t x;
    asm volatile("csrr %0, sscratch" : "=r"(x) );
    return x;
}

// スーパーバイザトラップ理由
static inline uint64_t
r_scause()
{
    uint64_t x;
    asm volatile("csrr %0, scause" : "=r" (x) );
    return x;
}

// スーパーバイザトラップ値
static inline uint64_t
r_stval()
{
    uint64_t x;
    asm volatile("csrr %0, stval" : "=r" (x) );
    return x;
}

// Machine-mode Counter-Enable
static inline void
w_mcounteren(uint64_t x)
{
    asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64_t
r_mcounteren()
{
    uint64_t x;
    asm volatile("csrr %0, mcounteren" : "=r" (x) );
    return x;
}

// r_time() / US_INTERVAL がマイクロ秒
static inline uint64_t
r_time()
{
    uint64_t x;
    asm volatile("csrr %0, time" : "=r" (x) );
    return x;
}

static inline uint64_t
r_cycle()
{
    uint64_t x;
    asm volatile("csrr %0, cycle" : "=r" (x) );
    return x;
}

// デバイスの割り込みを許可する
static inline void
intr_on()
{
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// デバイスの割り込みを無効にする
static inline void
intr_off()
{
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// デバイスは割り込みが許可されているか?
static inline int
intr_get()
{
    uint64_t x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

static inline uint64_t
r_sp()
{
    uint64_t x;
    asm volatile("mv %0, sp" : "=r" (x) );
    return x;
}

// スレッドポインタtpを読み書きする。xv6ではtpをこのコアのhartid
// (コア番号で、cpus[]のインデックスでもある）の保管に使用している => これは使用しない
static inline uint64_t
r_tp()
{
    uint64_t x;
    asm volatile("mv %0, tp" : "=r" (x) );
    return x;
}

static inline void
w_tp(uint64_t x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

static inline uint64_t
r_ra()
{
    uint64_t x;
    asm volatile("mv %0, ra" : "=r" (x) );
    return x;
}

// flush the TLB.
static inline void
sfence_vma()
{
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
}

static inline void
fence_i()
{
    asm volatile("fence.i");
}

static inline void
fence_rw()
{
    asm volatile("fence rw, rw");
}


typedef uint64_t pte_t;
typedef uint64_t *pagetable_t; // 512 PTEs

/* time: Timer for RDTIME instruction */
#define CSR_TIME		0xc01
/* sie: Supervisor Interrupt-enable register */
#define CSR_IE			0x104

#ifdef __ASSEMBLY__
#define __ASM_STR(x)    x
#else
#define __ASM_STR(x)    #x
#endif

#define csr_read(csr)                                           \
	({                                                      \
		register unsigned long __v;                     \
		__asm__ __volatile__("csrr %0, " __ASM_STR(csr) \
				     : "=r"(__v)                \
				     :                          \
				     : "memory");               \
		__v;                                            \
	})

#define csr_write(csr, val)                                        \
	({                                                         \
		unsigned long __v = (unsigned long)(val);          \
		__asm__ __volatile__("csrw " __ASM_STR(csr) ", %0" \
				     :                             \
				     : "rK"(__v)                   \
				     : "memory");                  \
	})

#define csr_clear(csr, val)                                        \
	({                                                         \
		unsigned long __v = (unsigned long)(val);          \
		__asm__ __volatile__("csrc " __ASM_STR(csr) ", %0" \
				     :                             \
				     : "rK"(__v)                   \
				     : "memory");                  \
	})

#define csr_set(csr, val)                                          \
	({                                                         \
		unsigned long __v = (unsigned long)(val);          \
		__asm__ __volatile__("csrs " __ASM_STR(csr) ", %0" \
				     :                             \
				     : "rK"(__v)                   \
				     : "memory");                  \
	})


#endif // __ASSEMBLER__

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // bits of offset within a page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define PTE_V       (1L << 0) // valid
#define PTE_R       (1L << 1)
#define PTE_W       (1L << 2)
#define PTE_X       (1L << 3)
#define PTE_U       (1L << 4) // user can access
#define PTE_G       (1L << 5)
#define PTE_A       (1L << 6)
#define PTE_D       (1L << 7)

#define PTE_SEC     (1UL << 59) /* Security */
#define PTE_S       (1UL << 60) /* Shareable */
#define PTE_B       (1UL << 61) /* Bufferable */
#define PTE_C       (1UL << 62) /* Cacheable */
#define PTE_SO      (1UL << 63) /* Strong Order */

#define PTE_TABLE(pte)   ((pte & 0xe) == 0)

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64_t)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte & ~((1UL << 63) | (1UL << 62) | (1UL << 61) | (1UL << 60) | (1UL << 59))) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & (0x3FF | (1UL << 63)|(1UL << 62) | (1UL << 61) | (1UL << 60) | (1UL << 59)))

// extract the three 9-bit page table indices from a virtual address.
/*
 * 仮想アドレス 'va' は次の3パートの構造をしている:
 * +-----9-----+-----9-----+-----9-----+---------12---------+
 * |  Level 2  |  Level 1  |  Level 0  | Offset within Page |
 * |   Index   |   Index   |   Index   |                    |
 * +-----------+-----------+-----------+--------------------+
 *  \PX(2, va) /\PX(1, va)/ \PTX(0, va)/
 *  [010000000] [000000000] [000000000] [0000 0000 0000]      = 0x2000000000
 *  [128]       [0]         [0]
 */

#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64_t) (va)) >> PXSHIFT(level)) & PXMASK)

// MAXVAは実際にはSv39が許容する最大値より1ビット小さいが
// これは上位ビットが設定されている仮想アドレスを符号拡張する
// 必要がないようにするためである
// = 0x40_0000_0000 = 256GB
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

#define PTE_THEAD_DEVICE	(PTE_SO | PTE_S)
#define PTE_THEAD_NORMAL	(PTE_C | PTE_B | PTE_S)
#define PTE_THEAD_NORMAL_NC	(PTE_B | PTE_S)

#include "config.h"
#ifdef THEAD_PTE
#define PTE_DEVICE	(PTE_R | PTE_W | PTE_THEAD_DEVICE)
#define PTE_EXEC	(PTE_R | PTE_X | PTE_THEAD_NORMAL)
#define PTE_RO		(PTE_R | PTE_THEAD_NORMAL)
#else
#define PTE_DEVICE	(PTE_R | PTE_W)
#define PTE_EXEC	(PTE_R | PTE_X)
#define PTE_RO		(PTE_R)
#endif
#define PTE_NORMAL	(PTE_RO | PTE_W)

#endif
