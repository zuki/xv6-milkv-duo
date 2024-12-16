#ifndef INC_PROC_H
#define INC_PROC_H

#include <common/types.h>
#include <common/param.h>
#include <linux/signal.h>
#include <spinlock.h>

// カーネルコンテキストスイッチ用に保存するレジスタ.
struct context {
    uint64_t ra;
    uint64_t sp;

    // callee-saved
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};

// CPUごとの状態.
struct cpu {
  struct proc *proc;          // このCPUで現在稼働中のプロセス、または null.
  struct context context;     // scheduler()に入るためにここにswtch()する.
  int noff;                   // push_off() ネスティングの深さ.
  int intena;                 // push_off()の前に割り込みが有効であったか?
};

extern struct cpu cpus[NCPU];

// trampoline.Sにあるトラップ処理コードのためのプロセスごとのデータ。
// ユーザページテーブルのtrampolineページのすぐ下のそれ自身のページに
// あり、カーネルページテーブルでは特にマッピングされていない。
// trampoline.Sにあるuservecがtrapframeにユーザレジスタを保存し、
// trapframeのkernel_sp、kernel_hartid、kernel_satpを使ってレジスタを
// 初期化し、kernel_trapにジャンプする。
// trampoline.Sにあるusertrapret()とuserretはtrapframeのkernel_*を
// セットアップし、trapframeからユーザレジスタを復元し、ユーザページ
// テーブルに切り替え、ユーザ空間に入る。
// usertrapret()を介したユーザへのリターンパスはカーネルコールスタックの
// すべてを通らずに復帰するのでtrapframeはs0-s11のようなcallee-savedな
// ユーザレジスタを含んでいる。
struct trapframe {
  /*   0 */ uint64_t kernel_satp;   // カーネルページテーブル
  /*   8 */ uint64_t kernel_sp;     // プロセスのカーネルスタック
  /*  16 */ uint64_t kernel_trap;   // usertrap()
  /*  24 */ uint64_t epc;           // ユーザプログラムカウンタを保存
  /*  32 */ uint64_t kernel_hartid; // カーネルのhartid（milkv-duoではプロセス構造体のアドレス）
  /*  40 */ uint64_t ra;
  /*  48 */ uint64_t sp;
  /*  56 */ uint64_t gp;
  /*  64 */ uint64_t tp;
  /*  72 */ uint64_t t0;
  /*  80 */ uint64_t t1;
  /*  88 */ uint64_t t2;
  /*  96 */ uint64_t s0;
  /* 104 */ uint64_t s1;
  /* 112 */ uint64_t a0;
  /* 120 */ uint64_t a1;
  /* 128 */ uint64_t a2;
  /* 136 */ uint64_t a3;
  /* 144 */ uint64_t a4;
  /* 152 */ uint64_t a5;
  /* 160 */ uint64_t a6;
  /* 168 */ uint64_t a7;
  /* 176 */ uint64_t s2;
  /* 184 */ uint64_t s3;
  /* 192 */ uint64_t s4;
  /* 200 */ uint64_t s5;
  /* 208 */ uint64_t s6;
  /* 216 */ uint64_t s7;
  /* 224 */ uint64_t s8;
  /* 232 */ uint64_t s9;
  /* 240 */ uint64_t s10;
  /* 248 */ uint64_t s11;
  /* 256 */ uint64_t t3;
  /* 264 */ uint64_t t4;
  /* 272 */ uint64_t t5;
  /* 280 */ uint64_t t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct signal {
    sigset_t mask;
    sigset_t pending;
    struct sigaction actions[NSIG];
};

// プロセスごとの状態
struct proc {
    struct spinlock lock;

    // 以下の項目を操作する場合はp->lockを保持する必要がある:
    enum procstate state;           // プロセスの状態
    void *chan;                     // 0でない場合、chanでsleep中
    int killed;                     // 0でない場合、プロセスはkillされた
    int paused;                     // 一時停止しているか
    int xstate;                     // 親のwait()に返すExit状態
    pid_t pid;                      // プロセス ID
    pid_t pgid;                     // プロセスグループ ID
    pid_t sid;                      // セッション ID
    uid_t uid;                      // ユーザーID
    gid_t gid;                      // グループID

    // 次の項目を使用する場合はwait_lockを保持する必要がある:
    struct proc *parent;            // 親プロセスへのポインタ

    // 以下の項目はプロセス私用なので操作の際にp->lockは不要
    uint64_t kstack;                // カーネルスタックの仮想アドレス
    uint64_t sz;                    // プロセスメモリサイズ（バイト単位）
    pagetable_t pagetable;          // ユーザページテーブル
    struct trapframe *trapframe;    // trampoline.S用のデータページへのポインタ
    struct context context;         // プロセスを実行するにはここにswtch()
    int fdflag;                     // fdフラグ（1ビット/ファイル）
    struct file *ofile[NOFILE];     // オープンしているフィル
    struct inode *cwd;              // カレントワーキングディレクトリ
    char name[16];                  // プロセス名（デバッグ用）
    struct signal signal;           // シグナル
    struct trapframe *oldtf;        // 旧trapframeを保存
};

#endif
