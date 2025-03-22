#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include <spinlock.h>
#include <proc.h>
#include <defs.h>
#include <config.h>
#include <printf.h>
#include <linux/mman.h>

extern char trampoline[], uservec[], userret[];

// kernelvec.S で kerneltrap() を呼び出す.
void kernelvec();

static int devintr();

void
trapinit(void)
{
    info("trapint ok");
}

// カーネルにいる間、例外とトラップを受けるように設定する.
void
trapinithart(void)
{
    // 割り込み/例外発生時はkernelvecにジャンプする
    w_stvec((uint64_t)kernelvec);
}

//
// ユーザ空間からの割り込み、例外、システムコールを処理する。
// trampoline.S から呼び出される
// この処理の間、pagetableはカーネルのpagetableであることに注意
//
void
usertrap(void)
{
    int which_dev = 0;
    int ret = 0;

    if ((r_sstatus() & SSTATUS_SPP) != 0)
        panic("usertrap: not from user mode");

    // 現在はカーネルにいるので
    // 割り込みと例外は kerneltrap() に送る
    w_stvec((uint64_t)kernelvec);

    struct proc *p = myproc();

    // sepcとstvalを保存
    uint64_t sepc = r_sepc();
    uint64_t stval = r_stval();
    uint64_t scause = r_scause();

    // 例外が発生したユーザpcを保存する.
    p->trapframe->epc = sepc;

    //which_dev = devintr();
    trace("scause: %d, which_dev: %d", scause, which_dev);

    if (scause == SCAUSE_EVCALL_USER) {
        // システムコール
        trace("1: scratch: 0x%016lx, tp: 0x%016lx, pid: %d, p: %p", r_sscratch(), r_tp(), p->pid, p);

        if (killed(p)) {
            int xstate = p->xstate ? p->xstate : -1;
            warn("scause(8): pid %d has been killed, xstate: %d", p->pid, xstate);
            exit(xstate);
        }

        // sepc はecall命令を指しているのでその次の命令に復帰するようにする
        p->trapframe->epc += 4;
        trace("epc: 0x%lx", p->trapframe->epc);
        // 割り込みは sepc, scause, sstatus を変更するが
        // これらのレジスタはもう使い終わったので割り込みを有効にする
        intr_on();
        syscall();
        trace("epc: 0x%lx, sp: 0x%lx, tp: 0x%lx, a0: 0x%lx", p->trapframe->epc, p->trapframe->sp, p->trapframe->tp, p->trapframe->a0);
    // 13: ページアクセス例外 (load)
    } else if (scause == SCAUSE_PAGE_LOAD) {
        trace("pid[%d] LOAD: addr=0x%lx on 0x%lx", p->pid, stval, sepc);
        if (alloc_mmap_page(p, stval, scause) < 0) {
            error("pid[%d] LOAD: alloc page failed", p->pid);
            setkilled(p);
        }
    // 15: ページアクセス例外 (store)
    } else if (scause == SCAUSE_PAGE_STORE) {
        trace("pid[%d] STORE: addr=0x%lx on 0x%lx", p->pid, stval, sepc);
        if ((ret = alloc_cow_page(p->pagetable, stval)) < 0) {
            error("pid[%d] COW: alloc page failed", p->pid);
            setkilled(p);
        } else if (ret == 1 && alloc_mmap_page(p, stval, scause) < 0) {
            error("pid[%d] STORE: alloc page failed", p->pid);
            setkilled(p);
        }
    } else if ((which_dev = devintr()) != 0) {
        // ok
    } else {
        debug("pid[%d]: scause %d sepc=0x%lx stval=0x%lx", p->pid, scause, sepc, stval);
        setkilled(p);
    }

    if (killed(p)) {
        int xstate = p->xstate ? p->xstate : -1;
        debug("pid[%d] was killed, xstate: %d", p->pid, xstate);
        exit(xstate);
    }

    // タイマー割り込みの場合はCPUを明け渡す.
    if (which_dev == 2) {
        yield();
    }

    // 保留中のシグナルを処理する
    check_pending_signal();

    usertrapret();
}

//
// ユーザ空間に復帰する
//
void
usertrapret(void)
{
    struct proc *p = myproc();

    // トラップ先をkerneltrap()からusertrap()に切り替える。
    // usertrap()が正しいユーザ空間に戻るまで割り込みを無効にする。
    intr_off();

    // システムコール、割り込み、例外の送り先をtrampoline.Sのuservecにする
    uint64_t trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
    w_stvec(trampoline_uservec);

    // プロセスが次にカーネルにトラップするときにuservecが必要とする
    // トラップフレーム値を設定する
    p->trapframe->kernel_satp = r_satp();         // カーネルページテーブル
    p->trapframe->kernel_sp = p->kstack + PGSIZE; // プロセスのカーネルスタック
    p->trapframe->kernel_trap = (uint64_t)usertrap;
    // FIXME: cpuは１つしか使わない。tpを別の目的で使用する
    //p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()
    p->trapframe->kernel_hartid = 0UL;

    // trampoline.Sのsretがユーザ空間に移動できるように
    // レジスタを設定する

    // S Previous PrivilegeモードをUserに設定する。
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // ユーザモードを示すようにSPPを0にクリアする
    x |= SSTATUS_SPIE; // ユーザモードでの割り込みを有効にする
    w_sstatus(x);

    // S Exception Program Counterに保存していたユーザPCをセットする
    w_sepc(p->trapframe->epc);

    // trampoline.Sに切り替えるユーザページテーブルを伝える
    uint64_t satp = MAKE_SATP(p->pagetable);

    // メモリの先頭にあるtrampoline.S内のuserret()を呼び出す。
    // この関数はユーザページテーブルに切り替え、ユーザレジスタを
    // 復元し、sretでユーザモードに切り替える
    uint64_t trampoline_userret = TRAMPOLINE + (userret - trampoline);
    ((void (*)(uint64_t))trampoline_userret)(satp);
}

// カーネルコードからの割り込みと例外は現在のカーネル
// スタックが何であれkernelvec経由でここに来る
void
kerneltrap()
{
    int which_dev = 0;
    uint64_t sepc = r_sepc();
    uint64_t sstatus = r_sstatus();
    uint64_t scause = r_scause();
    uint64_t stval = r_stval();

    if ((sstatus & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    if (intr_get() != 0)
        panic("kerneltrap: interrupts enabled");

    if ((which_dev = devintr()) == 0){
        debug("scause %d sepc=0x%lx stval=0x%lx", scause, sepc, stval);
        panic("kerneltrap");
    }

    // タイマー割り込みの場合はCPUを明け渡す.
    if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) {
        trace("yield");
        yield();
    }

    // yield()でトラップが発生した可能性があるので、kernelvec.Sの
    // sepc命令で使用できるようにトラップレジスタを復元する。
    w_sepc(sepc);
    w_sstatus(sstatus);
}


// 外部割り込みかソフトウェア割り込みかを判断して
// それを処理する。
// タイマー割り込みの場合は 2 を返す,
// その他のデバイスからの割り込みの場合は 1 を返す
// 認識できない場合は 0 を返す
static int devintr()
{
    uint64_t scause = r_scause();

    if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        // PLIC経由のスーバーバイザ外部割り込みである.

        // irq はどのデバイスが割り込んだかを示している.
        int irq = plic_claim();

        if (irq == UART0_IRQ) {
            //debug("uart");
            uartintr();
        //} else if(irq == VIRTIO0_IRQ){
            //  virtio_disk_intr();
        } else if (irq == SD0_IRQ) {
            //debug("sd0");
            //sd_intr();
        } else if (irq) {
            warn("unexpected interrupt irq=%d", irq);
        }

        // PLICは各デバイスが一度に最大1つしか割り込みの発生を
        // 許可していない。PLICにデバイスに再び割り込みを許可
        // するよう伝える。
        if (irq)
            plic_complete(irq);

        return 1;
#ifdef CONFIG_RISCV_M_MODE
    } else if (scause == 0x8000000000000001L) {
        // OpenSBIまたはkernelvec.Sのtimervecから転送された
        // マシンモードタイマー割り込みからのソフトウェア割り込み,

        if (cpuid() == 0){
            clockintr();
        }

        // sipのSSIPビットをクリアすることによりソフトウェア
        // 割り込みにacknowledgeする
        w_sip(r_sip() & ~2);
        return 2;
#else
    } else if ((scause & 0x8000000000000000L) && (scause & 0xff) == 5) {
        // S-modeのタイマー割り込み,
        unsigned long next;

        csr_clear(CSR_IE, 1 << 5);
        clockintr();
        next = csr_read(CSR_TIME) + INTERVAL;
        sbi_set_timer(next);
        csr_set(CSR_IE, 1 << 5);
        //debug("timer");

        return 2;
#endif
    } else {
        return 0;
    }
}
