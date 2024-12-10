//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <common/types.h>
#include <common/riscv.h>
#include <common/fs.h>
#include <defs.h>
#include <common/param.h>
#include <linux/stat.h>
#include <spinlock.h>
#include <proc.h>
#include <common/fs.h>
#include <sleeplock.h>
#include <common/file.h>
#include <linux/fcntl.h>
#include <printf.h>
#include <errno.h>
#include <pipe.h>

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    if (argint(n, &fd) < 0)
        return -1;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;
    if (pfd)
        *pfd = fd;
    if (pf)
        *pf = f;
    return 0;
}

// 指定されたファイル用のファイルディスクリプタを割り当てる.
// 成功時は呼び出し元からファイル参照を引き継ぐ.
int fdalloc(struct file *f, int from)
{
    int fd;
    struct proc *p = myproc();

    for (fd = from; fd < NOFILE; fd++){
        if (p->ofile[fd] == 0) {
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -EBADF;
}


static long dupfd(int fd, int from)
{
    struct file *f = myproc()->ofile[fd];
    if (!f) return -EBADF;

    filedup(f);
    return fdalloc(f, from);
}


long sys_dup(void)
{
    int fd;

    if (argint(0, &fd) < 0)
        return -EINVAL;
    return dupfd(fd, 0);

#if 0
    struct file *f;
    int fd;

    if (argfd(0, 0, &f) < 0)
        return -EINVAL;
    if ((fd = fdalloc(f, 0)) < 0)
        return -EBADF;
    filedup(f);
    debug("fd: %d, f.inum: %d, f.type: %d, f.major: %d", fd, f->ip->inum, f->type, f->major);
    return fd;
#endif
}

long sys_read(void)
{
    struct file *f;
    int n;
    uint64_t p;

    if (argu64(1, &p) < 0 || argint(2, &n) < 0)
        return -EINVAL;
    if (argfd(0, 0, &f) < 0)
        return -EBADF;
    return fileread(f, p, n, 1);
}

long sys_write(void)
{
    struct file *f;
    int n;
    uint64_t p;

    if (argu64(1, &p) < 0 || argint(2, &n) < 0)
        return -EINVAL;
    if (argfd(0, 0, &f) < 0)
        return -EBADF;

    return filewrite(f, p, n);
}

struct iovec {
    void *iov_base;             /* Starting address. */
    size_t iov_len;             /* Number of bytes to transfer. */
};

long sys_readv(void)
{
    struct file *f;
    int iovcnt;
    struct iovec *iov, *pp;

    if (argu64(1, (uint64_t *)&iov) < 0 || argint(2, &iovcnt) < 0)
        return -EINVAL;
    if (argfd(0, 0, &f) < 0)
        return -EBADF;

    ssize_t tot = 0;
    for (pp = iov; pp < iov + iovcnt; pp++) {
        tot += fileread(f, (uint64_t)pp->iov_base, pp->iov_len, 1);
    }
    return tot;
}

ssize_t sys_writev(void)
{
    struct file *f;
    int fd, iovcnt;
    struct iovec *iov, *pp, iov_k;
    struct proc *p = myproc();

    if (argu64(1, (uint64_t *)&iov) < 0 || argint(2, &iovcnt) < 0)
        return -EINVAL;
    trace("n: 1, iov: %p", iov);
    if (argfd(0, &fd, &f) < 0)
        return -EBADF;

    ssize_t tot = 0;
    for (pp = iov; pp < iov + iovcnt; pp++) {
#if 0
        if (!in_user(p->iov_base, p->iov_len)) {
            if (p->iov_base == 0 && p->iov_len == 0) continue;      // fflushで実行
            return -EFAULT;
        }
#endif
        if (copyin(p->pagetable, (char *)&iov_k, (uint64_t)pp, sizeof(struct iovec)) != 0)
            return -EIO;
        tot += filewrite(f, (uint64_t)iov_k.iov_base, iov_k.iov_len);
    }
    return tot;
}

long sys_close(void)
{
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0)
        return -EBADF;

    trace("fd: %d", fd);

    myproc()->ofile[fd] = 0;
    bit_remove(myproc()->fdflag, fd);
    fileclose(f);
    return 0;
}

long sys_fstat(void)
{
    struct file *f;
    uint64_t st; // user pointer to struct stat
    int fd;

    if (argu64(1, &st) < 0)
        return -EINVAL;
    if (argfd(0, &fd, &f) < 0)
        return -EBADF;
    trace("fd: %d, st: %p", fd, (struct st *)st);
    return filestat(f, st);
}

long sys_fstatat(void)
{
    char path[MAXPATH];
    uint64_t staddr; // user pointer to struct stat
    int dirfd, flags;

    struct inode *ip;
    struct stat st;

    if (argint(0, &dirfd) < 0 || argu64(2, &staddr) < 0
     || argint(3, &flags) < 0)
        return -EINVAL;
    if (argstr(1, path, MAXPATH) < 0)
        return -EFAULT;

    trace("dirfd: %d, path: %s, staddr: 0x%lx, flags: %d", dirfd, path, staddr, flags);

    /* 当面、無視する */
    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -EINVAL;
    }
    if (flags != 0) {
        warn("flags unimplemented: flags=%d", flags);
        return -EINVAL;
    }

    begin_op();
    if ((ip = namei(path)) == 0) {
        end_op();
        return -ENOENT;
    }
    ilock(ip);
    stati(ip, &st);
    iunlockput(ip);
    end_op();

    if (copyout(myproc()->pagetable, staddr, (char *)&st, sizeof(st)) < 0)
        return -EFAULT;
    return 0;
}

// Create the path new as a link to the same inode as old.
long sys_linkat(void)
{
    char newpath[MAXPATH], oldpath[MAXPATH];
    int oldfd, newfd, flags;

    if (argint(0, &oldfd) < 0 || argint(2, &newfd) < 0
     || argint(4, &flags) < 0
     || argstr(1, oldpath, MAXPATH) < 0
     || argstr(3, newpath, MAXPATH) < 0)
        return -EINVAL;

    trace("oldfd: %d, oldpath: %s, newfd: %d, newpath: %s, flags: %d", oldfd, oldpath, newfd, newpath, flags);

    // TODO: AT_FDCWD以外の実装
    if (oldfd != AT_FDCWD || newfd != AT_FDCWD)
        return -EINVAL;
    // TODO: AT_SYMLINK_FOLLOW は実装する
    if (flags)
        return -EINVAL;

    return filelink(oldpath, newpath);
}

long sys_unlinkat(void)
{
    char path[MAXPATH];
    int dirfd, flags;

    if (argint(0, &dirfd) < 0 || argint(2, &flags) < 0
     || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    if (dirfd != AT_FDCWD)
        return -EINVAL;

    if (flags & ~AT_REMOVEDIR)
        return -EINVAL;

    return fileunlink(path, flags);
}

long sys_openat(void)
{
    char path[MAXPATH];
    int dirfd, flags;
    mode_t mode;

    if (argint(0, &dirfd) < 0 || argint(2, &flags) < 0
     || argint(3, (int *)&mode) < 0
     || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -EINVAL;
    }
    trace("dirfd: %d, path: '%s', mode: 0x%08x, flags: 0x%08x", dirfd, path, mode, flags);
#if 0
    char *c = path;
    while (*c != 0) {
        printf("%02x", *c++);
    }
    printf("\n");
#endif
/*
    if ((flags & O_LARGEFILE) == 0) {
        error("expect O_LARGEFILE in open flags");
        return -EINVAL;
    }
*/
    return fileopen(path, flags, mode);
}

long sys_mkdirat(void)
{
    int dirfd;
    char path[MAXPATH];
    mode_t mode;
    struct inode *ip;

    if (argint(0, &dirfd) < 0 || argint(2, (int *)&mode) < 0
     || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    trace("dirfd %d, path '%s', mode 0x%x", dirfd, path, mode);

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -EINVAL;
    }


    begin_op();
    if ((long)(ip = create(path, T_DIR, 0, 0, mode | S_IFDIR)) == 0) {
        end_op();
        return (long)ip;
    }
    iunlockput(ip);
    end_op();
    return 0;
}

long sys_mknodat(void)
{
    struct inode *ip;
    char path[MAXPATH];
    int dirfd;
    mode_t mode;
    dev_t  dev;
    uint16_t major=0, minor=0, type;

    if (argint(0, &dirfd) < 0 || argint(2, (int *)&mode) < 0
     || argu64(3, &dev) < 0 || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -EINVAL;
    }
    trace("path '%s', mode 0x%x, dev 0x%lx", path, mode, dev);

    begin_op();
    if (S_ISDIR(mode))
        type = T_DIR;
    else if (S_ISREG(mode))
        type = T_FILE;
    else if (S_ISCHR(mode) || S_ISBLK(mode))
        type = T_DEVICE;
/*
    else if (S_ISLNK(mode))
        type = T_SYMLINK;
*/
    else {
        warn("%d is not supported yet", mode & S_IFMT);
        return -EINVAL;
    }
    trace("type: %d", type);
    if (type == T_DEVICE) {
        major = (uint16_t)((dev >> 8) & 0xFF);
        minor = (uint16_t)(dev & 0xFF);
        trace("dev: 0x%l016x, major: %d, minor: %d", dev, major, minor);
    }

    trace("path: %s, type: %d, major: %d, minor: %d, mode: %x", path, type, major, minor, mode);

    if ((long)(ip = create(path, type, major, minor, mode)) < 0) {
        end_op();
        return (long)ip;
    }
    iunlockput(ip);
    end_op();
    return 0;
}

long sys_chdir(void)
{
    char path[MAXPATH];
    struct inode *ip;
    struct proc *p = myproc();

    if(argstr(0, path, MAXPATH) < 0) {
        end_op();
        return -EINVAL;
    }

    begin_op();
    if ((ip = namei(path)) == 0) {
        end_op();
        return -ENOENT;
    }

    ilock(ip);

    if (ip->type != T_DIR){
        iunlockput(ip);
        end_op();
        return -ENOTDIR;
    }

    iunlock(ip);
    iput(p->cwd);
    end_op();
    p->cwd = ip;
    return 0;
}

long sys_execve(void)
{
    char path[MAXPATH], *argv[MAXARG];
    int i, error = 0;
    uint64_t uargv, uarg;

    if (argu64(1, &uargv) < 0)
        return -EINVAL;

    if (argstr(0, path, MAXPATH) < 0) {
        debug("parse path error");
        return -EINVAL;
    }
    memset(argv, 0, sizeof(argv));
    for (i = 0; ; i++) {
        if (i >= NELEM(argv)) {
            error("too many options");
            error = -E2BIG;
            goto bad;
        }
        if (fetchaddr(uargv + sizeof(uint64_t)*i, (uint64_t*)&uarg) < 0) {
            error("fetchaddr error: uarg[%d]", i);
            error = -EFAULT;
            goto bad;
        }
        if (uarg == 0) {
            argv[i] = 0;
            break;
        }
        // TODO: slabを使う
        argv[i] = kalloc();
        if (argv[i] == 0) {
            error("outof memory");
            error = -ENOMEM;
            goto bad;
        }

        if (fetchstr(uarg, argv[i], PGSIZE) < 0) {
            error("fetchstr error: argv[%d]", i);
            error = -EIO;
            goto bad;
        }

    }

    //int ret = exec(path, argv);
    trace("path: %s, argv[0]: %s", path, argv[0]);
    int ret = exec(path, argv);
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        kfree(argv[i]);

    return ret;

bad:
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        kfree(argv[i]);

    return error;
}

long sys_pipe2(void)
{
    uint64_t fdarray;       // fd[2]
    struct file *rf, *wf;
    int fd0, fd1, flags;
    struct proc *p = myproc();

    if (argint(1, &flags) < 0 || argu64(0, &fdarray) < 0
     || pipealloc(&rf, &wf, flags) < 0)
        return -ENFILE;

    if (flags & ~PIPE2_FLAGS) {
        warn("invalid flags=%d", flags);
        return -EINVAL;
    }

    fd0 = -1;
    if ((fd0 = fdalloc(rf, 0)) < 0 || (fd1 = fdalloc(wf, 0)) < 0){
        if (fd0 >= 0)
            p->ofile[fd0] = 0;
        fileclose(rf);
        fileclose(wf);
        return -EMFILE;
    }

    if (copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
        copyout(p->pagetable, fdarray + sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0) {
        p->ofile[fd0] = 0;
        p->ofile[fd1] = 0;
        bit_remove(p->fdflag, fd0);
        bit_remove(p->fdflag, fd1);
        fileclose(rf);
        fileclose(wf);
        return -EFAULT;
    }
    if (flags & O_CLOEXEC) {
        bit_add(p->fdflag, fd0);
        bit_add(p->fdflag, fd1);
    }
    return 0;
}

long sys_ioctl(void)
{
    struct file *f;
    uint64_t req;
    uint64_t argp;
    int fd;

    if (argu64(1, &req) < 0 || argu64(2, &argp) < 0
     || argfd(0, &fd, &f) < 0)
        return -EINVAL;

    trace("fd: %d, f.type: %d, f.major: %d, req: 0x%l016x, p: %p", fd, f->type, f->major, req, argp);

    if (f->type != FD_INODE && f->ip->type != T_DEVICE) {
        debug("bad type: %d, %d", f->type, f->ip->type);
        return -ENOTTY;
    }

    return fileioctl(f, req, (void *)argp);
}

long sys_fcntl(void)
{
    struct file *f;
    struct proc *p = myproc();
    int fd, cmd, args;

    if (argfd(0, &fd, &f) < 0
     || argint(1, &cmd) < 0 || argint(2, &args) < 0)
        return -EINVAL;

    trace("fd=%d, cmd=0x%x, args=%d", fd, cmd, args);

    switch (cmd) {
        case F_DUPFD:
            return dupfd(fd, args);

        case F_GETFD:
            return bit_test(p->fdflag, fd) ? FD_CLOEXEC : 0;

        case F_SETFD:
            if (args & FD_CLOEXEC)
                bit_add(p->fdflag, fd);
            else
                bit_remove(p->fdflag, fd);
            return 0;

        case F_GETFL:
            return (f->flags & (FILE_STATUS_FLAGS | O_ACCMODE));

        case F_SETFL:
            f->flags = ((args & FILE_STATUS_FLAGS) | (f->flags & O_ACCMODE));
            return 0;
    }

    return -EINVAL;
}

long sys_getdents64(void)
{
    int fd;
    char *dirp;
    uint64_t size;
    struct file *f;

    if (argfd(0, &fd, &f) < 0)
        return -EINVAL;

    if (argu64(1, (uint64_t *)&dirp) < 0 || argu64(2, &size) <0)
        return -EINVAL;

    trace("fd: %d, dirp: %p, count: %ld", fd, dirp, size);
    if (f->ip->type != T_DIR)
        return -ENOTDIR;

    return getdents(f, dirp, size);
}
