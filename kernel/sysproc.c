#include <common/types.h>
#include <common/riscv.h>
#include <defs.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <spinlock.h>
#include <proc.h>
#include <printf.h>
#include <errno.h>
#include <linux/time.h>
#include <linux/mman.h>
#include <linux/ppoll.h>
#include <linux/signal.h>
#include <common/file.h>

long sys_exit(void)
{
    int n;
    if (argint(0, &n) < 0)
        return -EINVAL;
    trace("pid[%d] exit with %d", myproc()->pid, n);
    exit(n);

    // never reached
    return 0;
}

long sys_exit_group(void)
{
    int n;
    if (argint(0, &n) < 0)
        return -EINVAL;
    trace("pid[%d] n: %d", myproc()->pid, n);
    exit(n);

    // not reached
    return 0;
}

long sys_getpid(void)
{
    pid_t pid = myproc()->pid;
    trace("pid: %d", pid)
    return pid;
}

long sys_getppid(void)
{
    return myproc()->parent->pid;
}

long sys_gettid(void)
{
    pid_t tid = myproc()->pid;
    trace("tid: %d", tid)
    return tid;
}

// long clone(unsigned long flags, void *child_stack, pid_t *ptid,
//            struct user_desc *tls, pid_t *ctid);
long sys_clone(void)
{
    void *childstk;
    uint64_t flag, tls;
    int ptid, ctid;

    if (argu64(0, &flag) < 0 || argu64(1, (uint64_t *) &childstk) < 0
     || argint(2, &ptid) < 0 || argu64(3, &tls) < 0
     || argint(4, &ctid) < 0)
        return -EINVAL;


    trace("flag: 0x%lx, cstk: %p, ptid: %d, tls: 0x%lx, ctid: 0x%lx", flag, childstk, ptid, tls, ctid);
    // FIXME: vforkの実装
    // flags = 0x4011の例あり(CLONE_VFORK | SIGCHLD)
    // FIXME: SIGCHLD(=17)のdefine
    if ((flag & 0xff) != SIGCHLD) {
        warn("flags other than SIGCHLD are not supported");
        return -1;
    }
    return fork();
}

// pid_t wait4(pid_t wpid, int *status, int options, struct rusage *rusage);
long sys_wait4(void)
{
    int wpid, options;
    uint64_t status, ru;

    if (argint(0, &wpid) < 0 || argu64(1, &status) < 0 ||argint(2, &options) < 0 || argu64(3, &ru) < 0)
        return -EINVAL;

    if (ru != 0)
        return -EINVAL;

    trace("wpid: %d, status: 0x%lx, options: 0x%x, ru: 0x%lx", wpid, status, options, ru);
    return wait4(wpid, status, options, ru);
}

long sys_brk(void)
{
    struct proc *p = myproc();
    long newsz, oldsz = (long)p->sz;

    if (argu64(0, (uint64_t *)&newsz) < 0)
        return -EINVAL;

    if (newsz < 0)
        return oldsz;

    trace("name: %s, newsz: 0x%lx, oldsz: 0x%lx", p->name, newsz, oldsz);

    if (newsz == 0)
        return oldsz;

    if (growproc((newsz - oldsz)) < 0)
        return oldsz;
    else
        return p->sz;
}

size_t sys_mmap(void)
{
    void *addr;
    size_t len, off;
    int prot, flags, fd;
    struct file *f;

    if (argu64(0, (uint64_t *) &addr) < 0 || argu64(1, &len) < 0
     || argint(2, &prot) < 0 || argint(3, &flags) < 0
     || argint(4, &fd) < 0 || argu64(5, &off) < 0)
        return -EINVAL;

    trace("addr: %p, len: %ld, prot: 0x%08x, flags: 0x%08x, fd: %d, off: %ld", addr, len, prot, flags, fd, off);

    if (flags & MAP_ANONYMOUS) {
        if (fd != -1) return -EINVAL;
        f = NULL;
    } else {
        if (fd < 0 || fd >= NOFILE) return -EBADF;
        if ((f = myproc()->ofile[fd]) == 0) return -EBADF;
    }

    if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0) {
        warn("invalid flags: 0x%x", flags);
        return -EINVAL;
    }

    if ((ssize_t)len <= 0 || (ssize_t)off < 0) {
        warn("invalid length: %ld or offset: %ld", len, off);
        return -EINVAL;
    }

    // MAP_FIXEDの場合、addrが指定されていなければならない
    if ((flags & MAP_FIXED) && addr == NULL) {
        warn("MAP_FIXED and addr is NULL");
        return -EINVAL;
    }

    // バックにあるファイルはreadableでなければならない
    if (!(flags & MAP_ANONYMOUS) && !f->readable) {
        warn("file is not readable");
        return -EACCES;
    }

    // MAP_SHAREかつPROT_WRITEの場合はバックにあるファイルがwritableでなければならない
    if (!(flags & MAP_ANONYMOUS) && (flags & MAP_SHARED)
     && (prot & PROT_WRITE) && !f->writable) {
        error("file is not writable");
        return -EACCES;
    }

    return mmap(addr, len, prot, flags, f, off);
}

long sys_munmap(void)
{
    void *addr;
    size_t len;

    if (argu64(0, (uint64_t *)&addr) < 0 || argu64(1, &len) < 0)
         return -EINVAL;

    trace("addr: 0x%llx, len: 0x%llx", addr, len);

    return munmap(addr, len);
}

long sys_msync(void)
{
    void *addr;
    size_t length;
    int flags;

    if (argu64(0, (uint64_t *)&addr) < 0 || argu64(1, &length) < 0
     || argint(2, &flags) < 0)
        return -EINVAL;

    return msync(addr, length, flags);

}

long sys_nanosleep(void)
{
    struct timespec req;
    uint64_t rem;
    uint64_t expire;
    struct timespec t = { 0, 0 };

    if (argu64(1, &rem) < 0)
        return -EINVAL;

    trace("req: n=0: 0x%lx, req: %p, size: %ld", myproc()->trapframe->a0, req, sizeof(struct timespec));
    if (argptr(0, (char *)&req, sizeof(struct timespec)) < 0)
        return -EINVAL;

    if (req.tv_nsec >= 1000000000L || req.tv_nsec < 0 || req.tv_sec < 0)
        return -EINVAL;

    expire = req.tv_sec * 1000000 + (req.tv_nsec + 999) / 1000;
    trace("sec: %d, nsec: %d, expire: %d", req.tv_sec, req.tv_nsec, expire);
    delayus(expire);

    if (rem) {
        if (copyout(myproc()->pagetable, rem, (char *)&t, sizeof(struct timespec)) < 0)
            return -EFAULT;
    }

    return 0;
}

long sys_kill(void)
{
    pid_t pid;
    int sig;

    if (argint(0, &pid) < 0 || argint(1, &sig) < 0)
        return -EINVAL;

    if (sig < 1 || sig >= NSIG)
        return -EINVAL;

    trace("pid=%d, sig=%d", pid, sig);

    return kill(pid, sig);
}

// return how many clock tick interrupts have occurred
// since start.
long sys_sysinfo(void)
{
    return -EINVAL;
}

// pid_t set_tid_address(int *tidptr);
long sys_set_tid_address(void)
{

    return myproc()->pid;
}

long sys_rt_sigsuspend(void)
{
    sigset_t mask;

    if (argptr(0, (char *)&mask, sizeof(sigset_t)) < 0)
        return -EINVAL;

    trace("p.mask: 0x%lx, mask: 0x%lx", myproc()->signal.mask, mask);

    return sigsuspend(&mask);
}

long sys_rt_sigaction()
{
    int sig;
    struct k_sigaction act;    // act: IN
    uint64_t oldact;            // struct sigaction *oldact: out

    if (argint(0, &sig) < 0 || argu64(2, &oldact) < 0) {
        warn("invalid argument: sig=%d, oldact=0x%lx", sig, oldact);
        return -EINVAL;
    }
    trace("sig=%d, oldact=0x%lx", sig, oldact);
    trace("act: n=1: 0x%lx, act: %p, size: %ld", myproc()->trapframe->a1, act, sizeof(struct k_sigaction));

    if (argptr(1, (char *)&act, sizeof(struct k_sigaction)) < 0) {
        warn("invalid argument: act=%p", &act);
        return -EINVAL;
    }

    if (sig < 1 || sig >= NSIG || sig == SIGSTOP || sig == SIGKILL) {
        warn("invalid sig %d", sig);
        return -EINVAL;
    }


    if (act.flags & SA_SIGINFO) {
        warn("not support siginfo");
        return -EINVAL;
    }

    return sigaction(sig, &act, oldact);
}

long sys_rt_sigpending()
{
    uint64_t pending;  // sigset_t *のアドレス: out

    if (argu64(0, &pending) < 0)
        return -EINVAL;

    trace("pendig: 0x%lx", pending);

    return sigpending(pending);
}

// int rt_sigprocmask(int how, const kernel_sigset_t *set, kernel_sigset_t *oldset, size_t sigsetsize);
long sys_rt_sigprocmask(void) {
    int how;
    sigset_t set;
    uint64_t oldset;
    size_t size;

    if (argint(0, &how) < 0 || argu64(2, &oldset) < 0 || argu64(3, &size) < 0)
        return -EINVAL;

    if (argptr(1, (char *)&set, sizeof(sigset_t)) < 0)
        return -EFAULT;

    trace("pid[%d] how=%d, set=0x%lx, &set: 0x%lx, oldset=0x%lx, size=%ld", myproc()->pid, how, set, &set, oldset, size);

    if (size && size != 8) {
        warn("unsupport sigset size: %ld", size);
        return -EINVAL;
    }

    return sigprocmask(how, &set, oldset);
}

long sys_rt_sigreturn(void)
{
    return sigreturn();
}

long sys_ppoll(void) {
    struct pollfd *fds;
    nfds_t nfds;
    struct timespec timeout_ts;
    sigset_t sigmask;

    //struct proc *p = myproc();
    trace("[0]: fds: 0x%lx, nfds: 0x%lx, timeout: 0x%lx, sigmask: 0x%lx", p->trapframe->a0, p->trapframe->a1, p->trapframe->a2, p->trapframe->a3);

    if (argu64(1, &nfds) < 0
     || argptr(2, (char *)&timeout_ts, sizeof(struct timespec)) < 0
     || argptr(3, (char *)&sigmask, sizeof(sigset_t)) < 0) {
        trace("invald either of nfds, timeout, sigmask");
        return -EINVAL;
     }


    if (nfds > 0) {
        // FIXME: kmallocを作成する
        fds = (struct pollfd *)kalloc();
        if (fds == 0) {
            error("no memory");
            return -ENOMEM;
        }

        if (argptr(0, (char *)fds, nfds * sizeof(struct pollfd)) < 0) {
            error("invalid fds");
            return -EINVAL;
        }

    } else {
        fds = NULL;
    }

    trace("fds: %p, nfds: %ld, timeout: 0x%lx, sigmask: 0x%lx", fds, nfds, &timeout_ts, sigmask);
    return ppoll(fds, nfds, &timeout_ts, &sigmask);
}

long sys_getpgid(void) {
    pid_t pid;

    if (argint(0, &pid) < 0)
        return -EINVAL;

    return getpgid(pid);
}

long sys_setpgid(void)
{
    pid_t pid, pgid;

    if (argint(0, &pid) < 0 || argint(1, &pgid) < 0)
        return -EINVAL;

    return setpgid(pid, pgid);
}
