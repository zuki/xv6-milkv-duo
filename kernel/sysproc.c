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
#include <common/file.h>

long sys_exit(void)
{
    int n;
    argint(0, &n);
    trace("n: %d", n);
    exit(n);
    return 0;  // not reached
}

long sys_exit_group(void)
{
    int n;
    argint(0, &n);
    trace("n: %d", n);
    exit(n);
    return 0;  // not reached
}

long sys_getpid(void)
{
    return myproc()->pid;
}

long sys_clone(void)
{
    void *childstk;
    uint64_t flag, tls;
    int ptid, ctid;

    argu64(0, &flag);
    argu64(1, (uint64_t *) &childstk);
    argint(2, &ptid);
    argu64(3, &tls);
    argint(4, &ctid);

    trace("flag: 0x%lx, cstk: %p, ptid: %d, tls: 0x%lx, ctid: %d", flag, childstk, ptid, tls, ctid);
    // FIXME: vforkの実装
    // flags = 0x4011の例あり(CLONE_VFORK | SIGCHLD)
    // FIXME: SIGCHLD(=17)のdefine
    if (flag != 17) {
        warn("flags other than SIGCHLD are not supported");
        return -1;
    }
    return fork();
}

long sys_wait4(void)
{
    int *wstatus;
    struct rusage *ru;
    int pid, options;
    uint64_t status;

    argint(0, &pid);
    argu64(1, (uint64_t *)&wstatus);
    argu64(1, &status);
    argint(2, &options);
    argu64(3, (uint64_t *)&ru);
    if (ru != 0)
        return -EINVAL;

    trace("pid: %d, wstatus: %p, status: 0x%lx, options: 0x%x, ru: %p", pid, wstatus, status, options, ru);
    return wait4(pid, wstatus, options, ru);
    //return wait(wstatus);
}

long sys_brk(void)
{
    struct proc *p = myproc();
    long newsz, oldsz = (long)p->sz;

    argu64(0, (uint64_t *)&newsz);
    if (newsz < 0)
        return oldsz;

    trace("name %s: 0x%llx to 0x%llx", p->name, oldsz, newsz);

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

    argu64(0, (uint64_t *) &addr);
    argu64(1, &len);
    argint(2, &prot);
    argint(3, &flags);
    argint(4, &fd);
    argu64(5, &off);

    debug("addr: %p, len: %ld, prot: 0x%08x, flags: 0x%08x, fd: %d, off: %ld", addr, len, prot, flags, fd, off);

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
        warn("file is not writable");
        return -EACCES;
    }

    if (addr) {
        if (prot != PROT_NONE) {
            warn("mmap unimplemented");
            return -EINVAL;
        }
        trace("map none at 0x%p", addr);
        return (size_t)addr;
    } else {
        if (prot != (PROT_READ | PROT_WRITE)) {
            warn("non-rw unimplemented");
            return -EINVAL;
        }
        error("unimplemented. ");
        return -EINVAL;
    }
}

long sys_nanosleep(void)
{
    struct timespec *req, *rem, t;
    uint64_t expire;

    argu64(0, (uint64_t *)&req);
    argu64(1, (uint64_t *)&rem);

    memmove(&t, req, sizeof(struct timespec));

    if (t.tv_nsec >= 1000000000L || t.tv_nsec < 0 || t.tv_sec < 0)
        return -EINVAL;

    expire = t.tv_sec * 1000000 + (t.tv_nsec + 999) / 1000;
    debug("sec: %d, nsec: %d, expire: %d", t.tv_sec, t.tv_nsec, expire);
    delayus(expire);

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return 0;
}

long sys_kill(void)
{
    int pid, sig;

    argint(0, &pid);
    argint(1, &sig);

    // TODO: SIGKILL以外のsignalの処理
    if (sig != 9)
         return -EINVAL;
    return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
long sys_sysinfo(void)
{
    return -EINVAL;
}

long sys_set_tid_address(void)
{
    return myproc()->pid;
}
