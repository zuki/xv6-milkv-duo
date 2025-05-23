        #
        # ユーザ空間からカーネルへのトラップを処理して
        # カーネルからユーザへ復帰する低レベルのコード
        #
        # カーネルはページテーブルを切り替えても動作し
        # 続けるように、このコードを保持するページを
        # ユーザ空間とカーネル空間の同じ仮想アドレス
        # （TRAMPOLINE）にマップする。
        # (kernel: vm.c#L49, user: proc.c#L215)
        # kernel.ldはこのコードをページ境界で開始させる。
        #

#include <common/riscv.h>
#include <common/memlayout.h>

.section trampsec
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:
	#
        # trap.cでstvecがここを指すように設定している(trap.c#L145-146)ので、
        # ユーザ空間からのトラップはここからスーパーバイザモードで
        # 開始される。ただし、ユーザページテーブルが使用される。

        # a0をTRAPFRAMEの取得に使用できるように
        # a0をsscratchに保存する
        csrw sscratch, a0

        # 各プロセスは独自のp->trapframeメモリ領域を持っているが、
        # それはすべてのプロセスのユーザページテーブルで同じ仮想
        # アドレス（TRAPFRAME）にマッピングされている。
        li a0, TRAPFRAME

        # ユーザレジスタをTRAPRAMEに保存する
        sd ra, 40(a0)
        sd sp, 48(a0)
        sd gp, 56(a0)
        sd tp, 64(a0)
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

	# sscratchに保存していたユーザa0をp->trapframe->a0に保管する
        csrr t0, sscratch
        sd t0, 112(a0)

        # p->trapframe->kernel_spの値からこのプロセス用のカーネルスタックポインタを初期化する
        ld sp, 8(a0)

        # tpはmuslでセットされる
        //ld tp, 32(a0)

        # p->trapframe->kernel_trapの値を使って、usertrap()のアドレスをロードする
        ld t0, 16(a0)

        # p->trapframe->kernel_satpの値を使って、カーネルページテーブルのアドレスをフェッチする
        ld t1, 0(a0)

        # ユーザページテーブルを使用したこれまでのメモリ操作が完了するのを待つ
        sfence.vma zero, zero

        # カーネルページテーブルをインストールする
        csrw satp, t1

        # 古くなったユーザエントリをTLBからフラッシュする
        sfence.vma zero, zero

        # usertrap()にジャンプする。この関数は戻らない
        jr t0

.globl userret
.balign 4
userret:
        # userret(pagetable)
        # カーネルからユーザに切り替えるために
        # trap.cのusertrapret()関数から呼び出される.
        # a0: satpにセットするユーザページテーブル.

        # switch to the user page table.
        sfence.vma zero, zero
        csrw satp, a0
        sfence.vma zero, zero

        li a0, TRAPFRAME

        # restore all but a0 from TRAPFRAME
        ld ra, 40(a0)
        ld sp, 48(a0)
        ld gp, 56(a0)
        ld tp, 64(a0)
        ld t0, 72(a0)
        ld t1, 80(a0)
        ld t2, 88(a0)
        ld s0, 96(a0)
        ld s1, 104(a0)
        ld a1, 120(a0)
        ld a2, 128(a0)
        ld a3, 136(a0)
        ld a4, 144(a0)
        ld a5, 152(a0)
        ld a6, 160(a0)
        ld a7, 168(a0)
        ld s2, 176(a0)
        ld s3, 184(a0)
        ld s4, 192(a0)
        ld s5, 200(a0)
        ld s6, 208(a0)
        ld s7, 216(a0)
        ld s8, 224(a0)
        ld s9, 232(a0)
        ld s10, 240(a0)
        ld s11, 248(a0)
        ld t3, 256(a0)
        ld t4, 264(a0)
        ld t5, 272(a0)
        ld t6, 280(a0)

	# restore user a0
        ld a0, 112(a0)

        fence.i
        # return to user mode and user pc.
        # usertrapret() set up sstatus and sepc.
        sret
