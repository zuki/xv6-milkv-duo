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
#include <linux/stat.h>
#include <linux/capability.h>
#include <linux/resources.h>

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

// int mprotect(void *addr, size_t len, int prot);
long sys_mprotect(void)
{
    void *addr;
    size_t len;
    int prot;

    if (argu64(0, (uint64_t *)&addr) < 0 || argu64(1, &len) < 0
     || argint(2, &prot) < 0)
        return -EINVAL;

    if ((uint64_t)addr & (PGSIZE-1))
        return -EINVAL;

    if ((prot & PROT_NONE) != 0 && (prot & (PROT_READ | PROT_WRITE | PROT_EXEC)) == 0 )
        return -EINVAL;

    trace("addr: %p, len: 0x%lx, prot: 0x%x", addr, len, prot);

    return mprotect(addr, len, prot);
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
        trace("invalid argument: sig=%d, oldact=0x%lx", sig, oldact);
        return -EINVAL;
    }
    trace("sig=%d, oldact=0x%lx", sig, oldact);
    trace("act: n=1: 0x%lx, act: %p, size: %ld", myproc()->trapframe->a1, act, sizeof(struct k_sigaction));

    if (argptr(1, (char *)&act, sizeof(struct k_sigaction)) < 0) {
        trace("invalid argument: act=%p", &act);
        return -EINVAL;
    }

    if (sig < 1 || sig >= NSIG || sig == SIGSTOP || sig == SIGKILL) {
        trace("invalid sig %d", sig);
        return -EINVAL;
    }


    if (act.flags & SA_SIGINFO) {
        trace("not support siginfo");
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

static inline void
cap_emulate_setxuid(int old_ruid, int old_euid, int old_suid)
{
    struct proc *p = myproc();

    if ((old_ruid == 0 || old_euid == 0 || old_suid == 0) &&
        (p->uid != 0 && p->euid != 0 && p->suid != 0)) {
        cap_clear(p->cap_permitted);
        cap_clear(p->cap_effective);
    }
    if (old_euid == 0 && p->euid != 0) {
        cap_clear(p->cap_effective);
    }
    if (old_euid != 0 && p->euid == 0) {
        p->cap_effective = p->cap_permitted;
    }
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

/* 実ユーザIDを取得する */
long sys_getuid()
{
    return myproc()->uid;
}

/* 実効ユーザID、保存ユーザIDを取得する */
long sys_geteuid()
{
    return myproc()->euid;
}

/* 実ユーザID、実効ユーザID、保存ユーザIDを取得する */
long sys_getresuid()
{
    struct proc *p = myproc();
    uint64_t ruid, euid, suid;

    if (argu64(0, &ruid) < 0 || argu64(1, &euid) < 0 || argu64(2, &suid) < 0)
        return -EINVAL;

    copyout(p->pagetable, ruid, (char *)&p->uid, sizeof(uid_t));
    copyout(p->pagetable, euid, (char *)&p->euid, sizeof(uid_t));
    copyout(p->pagetable, suid, (char *)&p->suid, sizeof(uid_t));

    return 0;
}

/* 実グループIDを取得する */
long sys_getgid()
{
    return myproc()->gid;
}

/* 実効グループIDを取得する */
long sys_getegid()
{
    return myproc()->egid;
}

/* 実グループID、実効グループID、保存グループIDを取得する */
long sys_getresgid()
{
    struct proc *p = myproc();
    uint64_t rgid, egid, sgid;

    if (argu64(0, &rgid) < 0 || argu64(1, &egid) < 0 || argu64(2, &sgid) < 0)
        return -EINVAL;

    copyout(p->pagetable, rgid, (char *)&p->gid, sizeof(gid_t));
    copyout(p->pagetable, egid, (char *)&p->egid, sizeof(gid_t));
    copyout(p->pagetable, sgid, (char *)&p->sgid, sizeof(gid_t));

    return 0;
}

/* ユーザIDを設定する */
long sys_setuid()
{
    struct proc *p = myproc();
    uid_t uid, old_ruid, old_euid, old_suid, new_ruid, new_suid;

    if (argint(0, (int *)&uid) < 0)
        return -EINVAL;

    old_euid = p->euid;
    old_ruid = new_ruid = p->uid;
    new_suid = old_suid = p->suid;

    trace("uid: %d, old: euid=%d ruid=%d suid=%d, new: ruid=%d suid=%d",
        uid, old_euid, old_ruid, old_suid, new_ruid, new_suid);
    trace("p[%d] cap_effective=%d", p->pid, p->cap_effective);


    if (capable(CAP_SETUID)) {
        if (uid != old_ruid) {
            p->uid = uid;
            new_suid = uid;
        } else if ((uid != p->uid) && (uid != new_suid)) {
            return -EPERM;
        }
    }

    if (old_euid != uid) {
        fence_i();
        fence_rw();
    }
    p->fsuid = p->euid = uid;
    p->suid = new_suid;

    cap_emulate_setxuid(old_ruid, old_euid, old_suid);

    return 0;
}

/* 実 (real)ユーザIDと実効 (effective)ユーザIDを設定する */
long sys_setreuid()
{
    struct proc *p = myproc();
    uid_t ruid, euid, old_ruid, old_euid , old_suid, new_ruid, new_euid;

    if (argint(0, (int *)&ruid) < 0 || argint(1, (int *)&euid) < 0)
        return -EINVAL;

    new_ruid = old_ruid = p->uid;
    new_euid = old_euid = p->euid;
    old_suid = p->suid;

    if (ruid != (uid_t)-1) {
        new_ruid = ruid;
        if ((old_ruid != ruid) && (p->euid != ruid) && !capable(CAP_SETUID))
            return -EPERM;
    }

    if (euid != (uid_t)-1) {
        new_euid = euid;
        if ((old_euid != euid) && (p->suid != euid) && !capable(CAP_SETUID))
            return -EPERM;
    }

    if (new_ruid != old_ruid)
        p->uid = ruid;

    if (new_euid != old_euid) {
        fence_i();
        fence_rw();
    }
    p->fsuid = p->euid = new_euid;

    if (ruid != (uid_t)-1 || (euid != (uid_t)-1 && euid != old_ruid))
        p->suid = p->euid;
    p->fsuid = p->euid;

    cap_emulate_setxuid(old_ruid, old_euid, old_suid);

    return 0;
}

/* ユーザの実ID、実効ID、保存IDを設定する */
long sys_setresuid()
{
    struct proc *p = myproc();
    uid_t ruid, euid, suid;
    uid_t old_ruid = p->uid, old_euid = p->euid, old_suid = p->suid;

    if (argint(0, (int *)&ruid) < 0 || argint(1, (int *)&euid) < 0
     || argint(2, (int *)&suid) < 0)
        return -EINVAL;

    if (!capable(CAP_SETUID)) {
        if ((ruid != (uid_t)-1) && (ruid != p->uid) &&
            (ruid != p->euid) && (ruid != p->suid))
            return -EPERM;
        if ((euid != (uid_t)-1) && (euid != p->uid) &&
            (euid != p->euid) && (euid != p->suid))
            return -EPERM;
        if ((suid != (uid_t)-1) && (suid != p->uid) &&
            (suid != p->euid) && (suid != p->suid))
            return -EPERM;
    }

    if (ruid != (uid_t)-1)
        p->uid = ruid;

    if (euid != (uid_t)-1) {
        if (euid != p->euid) {
            fence_i();
            fence_rw();
        }
        p->euid = euid;
        p->fsuid = euid;
    }
    if (suid != (uid_t)-1)
        p->suid = suid;

    cap_emulate_setxuid(old_ruid, old_euid, old_suid);

    return 0;
}

/* ファイルシステムのチェックに用いられるユーザIDを設定する */
long sys_setfsuid()
{
    struct proc *p = myproc();

    uid_t fsuid, old_fsuid = p->fsuid;

    if (argint(0, (int *)&fsuid) < 0)
        return -EINVAL;

    if (fsuid == p->uid || fsuid == p->euid || fsuid == p->suid
     || fsuid == p->fsuid || capable(CAP_SETUID)) {
        if (fsuid != old_fsuid) {
            fence_i();
            fence_rw();
        }
        p->fsuid = fsuid;
    }

    if (old_fsuid == 0 && p->fsuid != 0)
        cap_t(p->cap_effective) &= ~CAP_FS_MASK;
    if (old_fsuid != 0 && p->fsuid == 0)
        cap_t(p->cap_effective) |= (cap_t(p->cap_permitted) & CAP_FS_MASK);

    return old_fsuid;
}

/* グループIDを設定する */
long sys_setgid()
{
    struct proc *p = myproc();
    gid_t gid, old_egid = p->egid;

    if (argint(0, (int *)&gid) < 0)
        return -EINVAL;

    if (capable(CAP_SETGID)) {
        if (old_egid != gid) {
            fence_i();
            fence_rw();
        }
        p->gid = p->egid = p->sgid = gid;
    } else if (gid == p->gid || gid == p->sgid) {
        if (old_egid != gid) {
            fence_i();
            fence_rw();
        }
        p->egid = gid;
    } else {
        return -EPERM;
    }

    return 0;
}

/* 実グループIDと実効グループIDを設定する */
long sys_setregid()
{
    struct proc *p = myproc();
    gid_t rgid, egid;
    gid_t old_rgid = p->gid, old_egid = p->egid;
    gid_t new_rgid, new_egid;

    if (argint(0, (int *)&rgid) < 0 || argint(1, (int *)&egid) < 0)
        return -EINVAL;

    new_rgid = old_rgid;
    new_egid = old_egid;

    if (rgid != (gid_t)-1) {
        if (old_rgid == rgid || p->egid == rgid || capable(CAP_SETGID))
            new_rgid = rgid;
        else
            return -EPERM;
    }
    if (egid != (gid_t)-1) {
        if (old_rgid == egid || p->egid == egid || p->sgid == egid
          || capable(CAP_SETGID))
            new_egid = egid;
        else
            return -EPERM;
    }

    if (new_egid != old_egid) {
        fence_i();
        fence_rw();
    }
    if (rgid != (gid_t)-1 || (egid != (gid_t)-1 && egid != old_rgid))
        p->sgid = new_egid;
    p->fsgid = new_egid;
    p->egid = new_egid;
    p->gid = new_rgid;

    return 0;
}

/* 実グループID、実効グループID、保存グループIDを設定する */
long sys_setresgid()
{
    struct proc *p = myproc();
    gid_t rgid, egid, sgid;

    if (argint(0, (int *)&rgid) < 0 || argint(1, (int *)&egid) < 0
     || argint(2, (int *)&sgid) < 0)
        return -EINVAL;

    if (!capable(CAP_SETGID)) {
        if ((rgid != (gid_t)-1) && (rgid != p->gid) &&
            (rgid != p->egid) && (rgid != p->sgid))
            return -EPERM;
        if ((egid != (gid_t)-1) && (egid != p->gid) &&
            (egid != p->egid) && (egid != p->sgid))
            return -EPERM;
        if ((sgid != (gid_t)-1) && (sgid != p->gid) &&
            (sgid != p->egid) && (sgid != p->sgid))
            return -EPERM;
    }

    if (egid != (gid_t)-1) {
        if (egid != p->egid) {
            fence_i();
            fence_rw();
        }
        p->egid = egid;
        p->fsgid = egid;
    }

    if (rgid != (gid_t)-1)
        p->gid = rgid;
    if (sgid != (gid_t)-1)
        p->sgid = sgid;

    return 0;
}

/* ファイルシステムのチェックに用いられるグループIDを設定する */
long sys_setfsgid()
{
    struct proc *p = myproc();
    gid_t fsgid, old_fsgid = p->fsgid;

    if (argint(0, (int *)&fsgid) < 0)
        return -EINVAL;

    if (fsgid == p->gid || fsgid == p->egid || fsgid == p->sgid
     || fsgid == p->fsgid || capable(CAP_SETGID)) {
        if (fsgid != old_fsgid) {
            fence_i();
            fence_rw();
        }
        p->fsgid = fsgid;
    }

    return old_fsgid;
}

mode_t sys_umask()
{
    mode_t umask;
    mode_t oumask = myproc()->umask;

    if ((argint(0, (int *)&umask)) < 0) {
        return -EINVAL;
    }

    myproc()->umask = umask & S_IRWXUGO;
    return oumask;
}

long sys_sched_getaffinity(void) {
    pid_t pid;
    size_t cpusetsize;
    uint64_t maskp;
    cpu_set_t mask;

    if (argint(0, &pid) < 0 || argu64(1, &cpusetsize) < 0
     || argu64(2, &maskp) < 0)
        return -EINVAL;

    trace("pid: %d, size: 0x%lx, maskp: 0x%lx", pid, cpusetsize, maskp);

    mask.__bits[0] = 1UL;
    if (copyout(myproc()->pagetable, maskp, (char *)&mask, sizeof(cpu_set_t)) < 0)
            return -EINVAL;

    return 0;
}


long sys_prlimit64(void)
{
    pid_t pid;
    int resource;
    uint64_t new_limit_p, old_limit_p;
    struct rlimit old_limit;

    if (argint(0, &pid) < 0 || argint(1, &resource) < 0
     || argu64(2, &new_limit_p) < 0
     || argu64(3, &old_limit_p) < 0)
        return -EINVAL;

    trace("pid=%d, resource=%d, new_limit=0x%llx, old_limit=0x%llx",
        pid, resource, new_limit_p, old_limit_p);

    if (old_limit_p != 0) {
        old_limit.rlim_cur = RLIM_SAVED_CUR;
        old_limit.rlim_max = RLIM_SAVED_MAX;
        if (copyout(myproc()->pagetable, old_limit_p, (char *)&old_limit, sizeof(struct rlimit)) < 0)
            return -EINVAL;
    }

    return 0;

}
