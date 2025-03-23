#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include <spinlock.h>
#include <proc.h>
#include <defs.h>
#include <errno.h>
#include <printf.h>
#include <linux/resources.h>
#include <linux/wait.h>
#include <linux/ppoll.h>
#include <linux/time.h>

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct _q {
    struct spinlock lock;
    struct spinlock siglock;
} q;

struct proc *initproc;
struct slab_cache *MMAPREGIONS;

int nextpid = 1;
struct spinlock pid_lock;

static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// waitしている親の起床漏れがないようにする。
// p->parentを使っている際のメモリモデルに従う。
// p->lockする前に獲得しなければならない。
struct spinlock wait_lock;

// 各プロセス用のカーネルスタック用のページを割り当てる.
// トランポリン領域の下にプロセスごとに2ページ割り当て、
// 高位1ページはガードページで、下位1ページがスタック
// KSTACK(p) (TRAMPOLINE - ((p)+1) * 2 * PGSIZE) ; p = 0 - NPROC
void
proc_mapstacks(pagetable_t kpgtbl)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++) {
        char *pa = kalloc();
        if (pa == 0)
            panic("kalloc");
        // 2ページを割り当て
        uint64_t va = KSTACK((int) (p - proc));
        // 1ページ分だけマッピングする
        kvmmap(kpgtbl, va, (uint64_t)pa, PGSIZE, PTE_NORMAL);
    }
}

// プロセステーブルの初期化
void
procinit(void)
{
    struct proc *p;

    initlock(&pid_lock, "nextpid");
    initlock(&wait_lock, "wait_lock");
    initlock(&q.lock, "qlock");
    initlock(&q.siglock, "siglock");
    for (p = proc; p < &proc[NPROC]; p++) {
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        p->kstack = KSTACK((int) (p - proc));
    }

    MMAPREGIONS = slab_cache_create("mmap_region", sizeof(struct mmap_region), 0);
}

// 別のCPUに移されるプロセスとの競合を防ぐため、
// 割り込みを無効にして呼び出さなければならない。
int cpuid()
{
#if 0
    int id = r_tp();
    return id;
#endif
    return 0;
}

// このCPUのCPU構造体を返す。
// 割り込みは無効でなければならない。
struct cpu *mycpu(void)
{
    int id = cpuid();
    struct cpu *c = &cpus[id];
    return c;
}

// 現在稼働中のプロセスを返す。なければ0を返す.
struct proc *myproc(void)
{
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

// pidを割り当てる
static int allocpid()
{
    int pid;

    acquire(&pid_lock);
    pid = nextpid;
    nextpid = nextpid + 1;
    release(&pid_lock);

    return pid;
}

// 最初にフォークされた子のscheduler()によるスケジューリングは
// forkretにswtchされる。
static void forkret(void)
{
    static int first = 1;

    // まだschedulerからのp->lockを保持している.
    release(&myproc()->lock);

    if (first) {
        // ファイルシステムの初期化は通常プロセスのコンテキストで
        // 実行されなければならない（たとえば、sleepを呼び出すため）
        // ので、main()から実行することはできない。
        first = 0;
        fsinit(ROOTDEV);
    }
    usertrapret();
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state == UNUSED) {
            goto found;
        } else {
            release(&p->lock);
        }
    }

    return 0;

found:
    p->pid = allocpid();
    p->pgid = p->sid = p->pid;
    p->state = USED;
    p->regions = NULL;

    // trapframeページを割り当てる.
    if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // trampolinとtrapframeだけマップしたユーザページテーブルを作成する.
    p->pagetable = proc_pagetable(p);
    if (p->pagetable == 0) {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64_t)forkret;
    p->context.sp = p->kstack + PGSIZE;
    trace("allocproc pid: %d, addr: %p", p->pid, p);
    return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
    trace("pid: %d", p->pid);
    if (p->trapframe)
        kfree((void*)p->trapframe);
    p->trapframe = 0;
    free_mmap_list(p);
    p->regions = NULL;
    if (p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
    p->pagetable = 0;
    p->sz = 0;
    p->pid = 0;
    p->pgid = 0;
    p->sid = 0;
    p->uid = -1;
    p->gid = -1;
    p->fdflag = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->regions = NULL;
    memset(&p->signal, 0, sizeof(struct signal));
    p->state = UNUSED;
}

// 指定されたプロセスのユーザページテーブルを作成する。
// ここではユーザメモリは持たず、trampolineとtrapframe/tls
// 用のページだけをマッピングする。
pagetable_t
proc_pagetable(struct proc *p)
{
    pagetable_t pagetable;

    // 空のページテーブル.
    pagetable = uvmcreate();
    if (pagetable == 0)
        return 0;

    // トランポリンコード（システムコードからの復帰用）を
    // ユーザ仮想アドレスの最上位にマッピングする。
    // ユーザ空間との入出力にスーパーバイザだけが使用する
    // のでPTE_Uではない
    if (mappages(pagetable, TRAMPOLINE, PGSIZE,
                (uint64_t)trampoline, PTE_EXEC) < 0) {
        uvmfree(pagetable, 0);
        return 0;
    }

    // trampoline.S用にtrampolineページの直下にtrapframe
    // ページをマッピングする
    if (mappages(pagetable, TRAPFRAME, PGSIZE,
                (uint64_t)(p->trapframe), PTE_NORMAL) < 0) {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmfree(pagetable, 0);
        return 0;
    }

    return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64_t sz)
{
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode

/* envp = 0 */
unsigned char initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x85, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x65, 0x02,
  0x13, 0x06, 0x00, 0x00, 0x93, 0x08, 0xd0, 0x0d,
  0x73, 0x00, 0x00, 0x00, 0x93, 0x08, 0xd0, 0x05,
  0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0x9f, 0xff,
  0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* set envp[]
unsigned char initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x03, 0x35, 0x85, 0x07,
  0x97, 0x05, 0x00, 0x00, 0x83, 0xb5, 0x85, 0x07,
  0x17, 0x06, 0x00, 0x00, 0x03, 0x36, 0x86, 0x07,
  0x93, 0x08, 0xd0, 0x0d, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0xd0, 0x05, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x54, 0x5a, 0x3d, 0x4a, 0x53, 0x54,
  0x2d, 0x39, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x4b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
*/

// Set up first user process.
void
userinit(void)
{
    struct proc *p;

    p = allocproc();
    initproc = p;

    // allocate one user page and copy initcode's instructions
    // and data into it.
    uvmfirst(p->pagetable, initcode, sizeof(initcode));
    p->sz = PGSIZE;

    // prepare for the very first "return" from kernel to user.
    p->trapframe->epc = 0;      // user program counter
    p->trapframe->sp = PGSIZE;  // user stack pointer
    //p->trapframe->tp = p->trapframe->kernel_hartid

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");
    p->uid = p->gid = 0;

    p->state = RUNNABLE;

    release(&p->lock);
    trace("initproc pid: %d, addr: %p", initproc->pid, initproc);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
    uint64_t sz;
    struct proc *p = myproc();

    sz = p->sz;
    if (n > 0) {
        if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
            return -1;
        }
    } else if(n < 0){
        sz = uvmdealloc(p->pagetable, sz, sz + n);
    }
    p->sz = sz;
    return 0;
}

// 新しいプロセスを作成し、親プロセスをコピーする。ただし、
// ページを持つ部分はコピーしない。
// fork()システムコールから復帰するかのようにこのカーネルスタックをセットする.
int fork(void)
{
    int i, pid;
    struct proc *np;
    struct proc *p = myproc();
    int ret = 0;

    // 新規プロセスを割り当てる.
    if ((np = allocproc()) == 0) {
        return -ENOMEM;
    }

    // 親プロセスから子プロセスにmmap_regionsをコピーする
    if ((ret = copy_mmap_regions(p, np)) < 0) {
        //debug("ret=%d", ret);
        freeproc(np);
        release(&np->lock);
        error("failed copy_mmap_regions");
        return ret;
    }

    // 親プロセスから子プロセスにユーザメモリをコピーする.
    trace("uvmcopy pid[%d] to new_pid[%d]", p->pid, np->pid);
    if (uvmcopy(p, np) < 0) {
        freeproc(np);
        release(&np->lock);
        error("failed uvmcopy");
        return -ENOMEM;
    }

    np->sz = p->sz;
    np->pgid = p->pgid;
    np->sid = p->sid;
    np->uid = p->uid;
    np->gid = p->gid;

    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);

    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;

    // increment reference counts on open file descriptors.
    for (i = 0; i < NOFILE; i++)
        if (p->ofile[i])
            np->ofile[i] = filedup(p->ofile[i]);
    np->fdflag = p->fdflag;
    np->cwd = idup(p->cwd);
    memmove(&np->signal, &p->signal, sizeof(struct signal));

    safestrcpy(np->name, p->name, sizeof(p->name));

    pid = np->pid;

    release(&np->lock);

    acquire(&wait_lock);
    np->parent = p;
    release(&wait_lock);

    acquire(&np->lock);
    np->state = RUNNABLE;
    release(&np->lock);
    trace("pid: old: %d, new: %d", p->pid, pid);

    fence_i();
    fence_rw();

    return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
static void reparent(struct proc *p)
{
    struct proc *pp;

    for (pp = proc; pp < &proc[NPROC]; pp++) {
        if (pp->parent == p) {
            debug("pid[%d] reparent", pp->pid);
            pp->parent = initproc;
            wakeup(initproc);
        }
    }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
    struct proc *p = myproc();
    trace("pid: %d", p->pid);

    if (p == initproc)
        panic("init exiting");

    // mappingを解除する
    //print_mmap_list(p, "before exit");
    if (p->regions) {
        struct mmap_region *region = p->regions;
        while (region) {
            if (region->f)
                trace("pid[%d] f->ip: %d refcnt[1]: %d", p->pid, region->f->ip->inum, region->f->ip->ref);
            munmap(region->addr, region->length);
            region = region->next;
        }
        p->regions = NULL;
    }
    //print_mmap_list(p, "after  exit");

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd]) {
            struct file *f = p->ofile[fd];
            trace("pid[%d] f->ip: %d refcnt[2]: %d", p->pid, f->ip->inum, f->ip->ref);
            fileclose(f);
            p->ofile[fd] = 0;
        }
    }
    p->fdflag = 0;

    begin_op();
    iput(p->cwd);
    end_op();
    p->cwd = 0;

    acquire(&wait_lock);

    // Give any children to init.
    reparent(p);

    // Parent might be sleeping in wait().
    trace("pid[%d] wakup parent[%d]", p->pid, p->parent->pid);
    wakeup(p->parent);

    acquire(&p->lock);

    p->xstate = status;
    p->state = ZOMBIE;

    release(&wait_lock);

    // Jump into the scheduler, never to return.
    trace("pid[%d] xstate=0x%x, state=0x%x", p->pid, p->xstate, p->state);
    sched();
    panic("zombie exit");
}

// 子プロセスがexitするのを待ち、そのpidを返す.
// このプロセスが子を持たいない場合、-1 を返す。
int
wait4(pid_t pid, uint64_t status, int options, uint64_t ru)
{
    struct proc *pp;
    int rpid, kids = 0, xstate;
    struct proc *p = myproc();

    acquire(&wait_lock);
    while(1) {
        // プロセステーブルを走査してexitした子プロセスを見つける
        for (pp = proc; pp < &proc[NPROC]; pp++) {
            if (pp->parent != p) continue;
            if (pid > 0) {
                if (pp->pid != pid) continue;
            } else if (pid == 0) {
                if (pp->pgid != p->pgid)
                    continue;
            } else if (pid != -1) {
                if (pp->pgid != -pid)
                    continue;
            }
            // make sure the child isn't still in exit() or swtch().
            acquire(&pp->lock);
            kids = 1;
            if (pp->state == ZOMBIE
                || (options & WUNTRACED && pp->state == SLEEPING)
                || (options & WNOHANG)) {
                if (status) {
                    // status = xstatus << 8 || 0x0 (終了ステータス | exitで終了)
                    xstate = ((pp->xstate & 0xff) << 8);
                    if (copyout(p->pagetable, status, (char *)&xstate,
                                        sizeof(int)) < 0) {
                        release(&pp->lock);
                        release(&wait_lock);
                        return -EFAULT;
                    }
                }
                if (ru) {
                    struct rusage rusage;
                    if (copyout(p->pagetable, ru, (char *)&rusage,
                                        sizeof(struct rusage)) < 0) {
                        release(&pp->lock);
                        release(&wait_lock);
                        return -EFAULT;
                    }
                }
                rpid = pp->pid;
                freeproc(pp);
                release(&pp->lock);
                release(&wait_lock);
                trace("pid[%d] was exit: xstate: 0x%x", rpid, xstate);
                return rpid;
            }
            release(&pp->lock);
        }
        // pは子を持たない、またはpがkillされた
        if (!kids || killed(p)) {
            release(&wait_lock);
            return -ECHILD;
        }

        // Wait for a child to exit.
        trace("pid[%d] sleep for exit", p->pid)
        sleep(p, &wait_lock);  //DOC: wait-sleep
    }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();
    int found;

    c->proc = 0;
    for(;;) {
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        found = 0;

        for(p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                // Switch to chosen process.  It is the process's job
                // to release its lock and then reacquire it
                // before jumping back to us.
                found = 1;
                p->state = RUNNING;
                c->proc = p;
                trace("switch to %d", p->pid);
                swtch(&c->context, &p->context);

                // Process is done running for now.
                // It should have changed its p->state before coming back.
                c->proc = 0;
            }
            release(&p->lock);
        }

        // Wait for interrupt if no runnable process is found.
        // Otherwise there would be a high cpu usage.
        if (!found) {
            intr_on();
            asm volatile("wfi");
        }
    }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
    int intena;
    struct proc *p = myproc();

    if(!holding(&p->lock))
        panic("sched p->lock");
    if(mycpu()->noff != 1)
        panic("sched locks");
    if(p->state == RUNNING)
        panic("sched running");
    if(intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
    struct proc *p = myproc();
    acquire(&p->lock);
    p->state = RUNNABLE;
    sched();
    release(&p->lock);
}



// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);

    // Go to sleep.
    p->chan = chan;
    p->state = SLEEPING;

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    release(&p->lock);
    acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++) {
        if (p != myproc()) {
            acquire(&p->lock);
            if (p->state == SLEEPING && p->chan == chan) {
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}

// プロセスを停止する
static void term_handler(struct proc *p)
{
    trace("pid=%d", p->pid);
    acquire(&p->lock);
    p->killed = 1;
    if (p->state == SLEEPING)
        p->state = RUNNABLE;

    release(&p->lock);
}

// プロセスを継続する
static void cont_handler(struct proc *p)
{
    trace("pid=%d", p->pid);
    wakeup(p);
}

// プロセスを停止する
static void stop_handler(struct proc *p)
{
    trace("pid=%d", p->pid);
    acquire(&q.lock);
    sleep(p, &q.lock);
    release(&q.lock);
}

// ユーザハンドラを処理する
static void user_handler(struct proc *p, int sig)
{
    trace("sig=%d", sig);

    // now change the eip to point to the user handler
    trace("act[%d].handler: %p", sig, p->signal.actions[sig].sa_handler);
    p->trapframe->epc = (uint64_t)p->signal.actions[sig].sa_handler;
}

// シグナルを処理する
static void handle_signal(struct proc *p, int sig)
{
    trace("[%d]: sig=%d, handler=0x%lx", p->pid, sig, p->signal.actions[sig].sa_handler);

    if (!sig) return;

    if (p->signal.actions[sig].sa_handler == SIG_IGN) {
        trace("sig %d handler is SIG_IGN", sig);
    } else if (p->signal.actions[sig].sa_handler == SIG_DFL) {
        switch(sig) {
            case SIGSTOP:
            case SIGTSTP:
            case SIGTTIN:
            case SIGTTOU:
                stop_handler(p);
                break;
            case SIGCONT:
                cont_handler(p);
                break;
            case SIGABRT:
            case SIGBUS:
            case SIGFPE:
            case SIGILL:
            case SIGQUIT:
            case SIGSEGV:
            case SIGSYS:
            case SIGTRAP:
            case SIGXCPU:
            case SIGXFSZ:
                // Core: through
            case SIGALRM:
            case SIGHUP:
            case SIGINT:
            case SIGIO:
            case SIGKILL:
            case SIGPIPE:
            case SIGPROF:
            case SIGPWR:
            case SIGTERM:
            case SIGSTKFLT:
            case SIGUSR1:
            case SIGUSR2:
            case SIGVTALRM:
                p->xstate = (sig & 0x7f);
                term_handler(p);
                break;
            case SIGCHLD:
            case SIGURG:
            case SIGWINCH:
                // Doubt - ignore handler()
                break;
            default:
                break;
        }
    } else {
        trace("call user_handler: sig = %d", sig);
        user_handler(p, sig);
    }

    // clear the pending signal flag
    acquire(&q.siglock);
    sigdelset(&p->signal.pending, sig);
    release(&q.siglock);
}

// IDがpidのプロセスにシグナルsigを送信する
void send_signal(struct proc *p, int sig)
{
    trace("pid=%d, sig=%d, state=%d, paused=%d", p->pid, sig, p->state, p->paused);
    if (sig == SIGKILL) {
        p->killed = 1;
    } else {
        if (!sigismember(&p->signal.pending, sig))
            sigaddset(&p->signal.pending, sig);
        //else
            //debug("sig already pending");
    }

    if (p->state == SLEEPING) {
        if (p->paused == 1 && (sig == SIGTERM || sig == SIGINT || sig == SIGKILL)) {
            // For process which are SLEEPING by pause()
            p->paused = 0;
            handle_signal(p, SIGCONT);
        } else if (p->paused == 0 && p->killed != 1) {
            // For stopped process
            handle_signal(p, sig);
        }
    }
}

// 与えられたpidを持つプロセスを殺す。
// 犠牲者は、ユーザ空間に戻ろうとするまで
// 終了しない(trap.cのusertrap()を参照)。
int kill(pid_t pid, int sig)
{
    struct proc *p, *cp = myproc();
    pid_t pgid;
    int err = -EINVAL;

    if (pid == 0 || pid < -1) {
        pgid = pid == 0 ? cp->pgid : -pid;
        if (pgid > 0) {
            err = -ESRCH;
            for (p = proc; p < &proc[NPROC]; p++) {
                if (p->pgid == pgid) {
                    send_signal(p, sig);
                    err = 0;
                }
            }
        }
    } else if (pid == -1) {
        for (p = proc; p < &proc[NPROC]; p++) {
            if (p->pid > 1 && p != cp) {
                send_signal(p, sig);
                err = 0;
            }
        }
    } else {
        for (p = proc; p < &proc[NPROC]; p++) {
            if (p->pid == pid) {
                send_signal(p, sig);
                err = 0;
            }
        }
    }
    return err;
}

void
setkilled(struct proc *p)
{
    acquire(&p->lock);
    p->killed = 1;
    release(&p->lock);
}

int
killed(struct proc *p)
{
    int k;

    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64_t dst, void *src, uint64_t len)
{
    struct proc *p = myproc();
    if (user_dst) {
        return copyout(p->pagetable, dst, src, len);
    } else {
        memmove((char *)dst, src, len);
        return 0;
    }
}

/*
 * usr_srcに応じて、ユーザーアドレスまたはカーネルアドレスから
 * コピーする。
 * 成功すれば0を返し、エラーなら-1を返す。
 */
int either_copyin(void *dst, int user_src, uint64_t src, uint64_t len)
{
    struct proc *p = myproc();
    if (user_src) {
        return copyin(p->pagetable, dst, src, len);
    } else {
        memmove(dst, (char*)src, len);
        return 0;
    }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

// 10ミリ秒単位のdelay
void
kdelay(unsigned long n)
{
  uint64_t t0 = r_time();

  while(r_time() - t0 < n * INTERVAL){
    asm volatile("nop");
  }
}

void
delayms(unsigned long n)
{
  delayus(n * 1000UL);
}

void
delayus(unsigned long n)
{
  uint64_t t0 = r_time();

  while(r_time() - t0 < n * US_INTERVAL){
    asm volatile("nop");
  }
}

// startからの経過時間をms単位で返す
uint64_t get_timer(uint64_t start)
{
    return (r_time() / US_INTERVAL) / 1000 - start;
}

// sys_rt_sigsuspendの実装
long sigsuspend(sigset_t *mask)
{
    struct proc *p = myproc();
    sigset_t oldmask;

    acquire(&q.siglock);
    p->paused = 1;
    sigdelset(mask, SIGKILL);
    sigdelset(mask, SIGSTOP);
    oldmask = p->signal.mask;
    siginitset(&p->signal.mask, mask);
    release(&q.siglock);

    acquire(&q.lock);
    sleep(p, &q.lock);
    release(&q.lock);

    acquire(&q.siglock);
    p->signal.mask = oldmask;
    release(&q.siglock);
    return -EINTR;
}

// sys_rt_sigactionの実装

long sigaction(int sig, struct k_sigaction *act,  uint64_t oldact)
{
    acquire(&q.siglock);
    struct signal *signal = &myproc()->signal;

    if (oldact) {
        struct sigaction *action = &signal->actions[sig];
        struct k_sigaction kaction;
        kaction.handler = action->sa_handler;
        kaction.flags = (unsigned long)action->sa_flags;
        memmove((void *)&kaction.mask, &action->sa_mask, 8);
        if (copyout(myproc()->pagetable, oldact, (char *)&kaction, sizeof(struct k_sigaction)) < 0)
            return -EFAULT;
    }

    if (act) {
        struct sigaction *action = &signal->actions[sig];
        action->sa_handler = act->handler;
        action->sa_flags = (int)act->flags;
        memmove((void *)&action->sa_mask, &act->mask, 8);
        signal->mask = action->sa_mask;
        sigdelset(&signal->mask, SIGKILL);
        sigdelset(&signal->mask, SIGSTOP);
    }
    release(&q.siglock);

    return 0;
}

// sys_rt_sigpendingの実装
long sigpending(uint64_t pending)
{
    long err = 0;

    acquire(&q.siglock);
    struct proc *p = myproc();
    if (copyout(p->pagetable, pending, (char *)&p->signal.pending, sizeof(sigset_t)) < 0)
        err = -EFAULT;
    release(&q.siglock);
    return err;
}

// sys_rt_sigprocmaskの実装
long sigprocmask(int how, sigset_t *set, uint64_t oldset)
{
    long ret = 0;

    acquire(&q.siglock);
    struct signal *signal = &myproc()->signal;
    if (oldset) {
        if (copyout(myproc()->pagetable, oldset, (char *)&signal->mask, sizeof(sigset_t)) < 0)
            return -EFAULT;
    }

    if (set) {
        switch(how) {
            case SIG_BLOCK:
                sigorset(&signal->mask, &signal->mask, set);
                break;
            case SIG_UNBLOCK:
                signotset(set, set);
                sigandset(&signal->mask, &signal->mask, set);
                break;
            case SIG_SETMASK:
                siginitset(&signal->mask, set);
                break;
            default:
                ret = -EINVAL;
        }
    }
    trace("pid[%d]: newmask=0x%lx", myproc()->pid, signal->mask);
    release(&q.siglock);
    return ret;
}

// sys_rt_sigreturnの実装
//   signal_handler処理後の後始末をする
long sigreturn(void)
{
    memmove((void *)myproc()->trapframe, (void *)myproc()->oldtf, sizeof(struct trapframe));
    return 0;
}

// trap処理終了後、ユーザモードに戻る前に実行される
void check_pending_signal(void)
{
    struct proc *p = myproc();

    for (int sig = 0; sig < NSIG; sig++) {
        if (sigismember(&p->signal.pending, sig) == 1) {
            trace("pid=%d, sig=%d", p->pid, sig);
            handle_signal(p, sig);
            break;
        }
    }
}

long ppoll(struct pollfd *fds, nfds_t nfds, struct timespec *timeout_ts, sigset_t *sigmask) {
    struct proc *p = myproc();
    sigset_t origmask = p->signal.mask;

    if (fds == NULL) {
        p->paused = 1;
        acquire(&q.lock);
        sleep(p, &q.lock);
        release(&q.lock);
    }

    // FIXME:
    for (int i = 0; i < nfds; i++) {
        fds[i].revents = fds[i].fd == 0 ? POLLIN : POLLOUT;
    }

    siginitset(&p->signal.mask, &origmask);

    if (fds)
        kfree(fds);

    // reventsを持つfdsの数を返す。0はtimeout
    return nfds;
}

long setpgid(pid_t pid, pid_t pgid)
{
    struct proc *cp = myproc(), *p = 0, *pp;
    long error = -EINVAL;

    if (!pid) pid = cp->pid;
    if (!pgid) pgid = pid;
    if (pgid < 0) return -EINVAL;

    if (pid != cp->pid) {
        for (pp = proc; pp < &proc[NPROC]; pp++) {
            acquire(&pp->lock);
            if (pp->pid == pid) {
                p = pp;
                release(&pp->lock);
                break;
            }
            release(&pp->lock);
        }
        error = -ESRCH;
        if (!p) goto out;
    } else {
        p = cp;
    }

    error = -EINVAL;
    if (p->parent == cp) {
        error = -EPERM;
        if (p->sid != cp->sid) goto out;
    } else {
        error = -ESRCH;
        if (p != cp) goto out;
    }

    if (pgid != pid) {
        for (pp = proc; pp < &proc[NPROC]; pp++) {
            acquire(&pp->lock);
            if (pp->sid == cp->sid) {
                release(&pp->lock);
                goto ok_pgid;
            }
            release(&pp->lock);
        }
        goto out;
    }

ok_pgid:
    if (cp->pgid != pgid) {
        cp->pgid = pgid;
    }
    error = 0;
out:
    return error;
}

pid_t getpgid(pid_t pid)
{
    struct proc *p;

    if (!pid) {
        return myproc()->pgid;
    } else {
        for (p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->pid == pid) {
                release(&p->lock);
                return p->pgid;
            }
            release(&p->lock);
        }
        return -ESRCH;
    }
}
