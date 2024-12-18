#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include "defs.h"

void main();
void timerinit();

// entry.S はCPU事に1つスタックを必要とする.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

#ifdef CONFIG_RISCV_M_MODE
// マシンモードタイマー割り込み用のCPUごとのスクラッチ領域。
uint64_t timer_scratch[NCPU][5];
#endif

// kernelvec.Sにあるマシンモードタイマー割り込み用のアセンブリコード。
extern void timervec();

// entry.Sは、stack0上でスーパーバイザ／マシンモードでここにジャンプする
void
start(unsigned long hartid, unsigned long fdt)
{
#ifdef CONFIG_RISCV_M_MODE
    // mretでスーパーバイザに復帰するようにM-以前の権限モードをスーパーバイザとする
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // mretでmainに復帰するようにM-例外プログラムカウンタをmainとする
    // これにはgccで -mcmodel=medany のオプションが必要
    w_mepc((uint64_t)main);
#endif

    // ページングを無効にする.
    w_satp(0);
    sfence_vma();

#ifdef CONFIG_RISCV_M_MODE
    // すべての例外をスーパーバイザモードに委譲する.
    w_medeleg(0xffff);
    // スーパーバイザ割り込みをスーバーバイザモードに委譲する.
    w_mideleg(0x222);
#endif
    // 外部、タイマー、ソフトウェア割り込みを有効にする
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

#ifdef CONFIG_RISCV_M_MODE
    // スーパーバイザモードがすべての物理メモリにアクセス
    // できるように Physical Memory Protection を構成する.
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    // クロック割り込みを要求する.
    timerinit();

    // cpuid()用に各CPUのhartidをそのtpレジスタに保持する。.
    //int id = r_mhartid();
    //w_tp(id);

    // スーパーバイザモードに切り替えて、main()にジャンプする.
    asm volatile("mret");
#endif
    //w_tp(hartid);
    main();
}

// タイマ割り込みを受け取るようにする。
// この割り込みはマシンモードでkernelvec.Sの
// timervecに到着する。これをtrap.cのdevintr()で
// 受け取れるようにソフトウェア割り込みに変換する
#ifdef CONFIG_RISCV_M_MODE
void
timerinit()
{
    // CPUは各自専用のタイマー割り込みソースを持つ
    int id = r_mhartid();

    // CLINTにタイマー割り込みを要求する。.
    int interval = 1000000; // cycles; about 1/10th second in qemu.
    *(uint64_t*)CLINT_MTIMECMP(id) = *(uint64_t*)CLINT_MTIME + interval;

    // timervec用のscratch[]に収める情報を準備する
    // scratch[0..2] : レジスタを保存するためのスペース.
    // scratch[3] : CLINT MTIMECMPレジスタのアドレス.
    // scratch[4] : タイマー割り込みの間隔（サイクル単位）.
    uint64_t *scratch = &timer_scratch[id][0];
    scratch[3] = CLINT_MTIMECMP(id);
    scratch[4] = interval;
    w_mscratch((uint64_t)scratch);

    // マシンモードトラップハンドラをセットする.
    w_mtvec((uint64_t)timervec);

    // マシンモードの割り込みを有効にする.
    w_mstatus(r_mstatus() | MSTATUS_MIE);

    // マシンモードのタイマー割り込みを有効にする.
    w_mie(r_mie() | MIE_MTIE);
}
#endif
