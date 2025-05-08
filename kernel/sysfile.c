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
#include <linux/time.h>
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

// AT_FDCWDをチェックする
static int check_fdcwd(const char *path, int dirfd)
{
   if (*path != '/' && dirfd != AT_FDCWD) {
        if (myproc()->ofile[dirfd] == 0)
            return -EBADF;
        if (myproc()->ofile[dirfd]->type != T_DIR)
            return -ENOTDIR;
    }

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

long sys_dup3()
{
    struct file *f;
    int fd1, fd2, flags;
    struct proc *p = myproc();

     if (argint(0, &fd1) < 0 || argint(1, &fd2) < 0 || argint(2, &flags) < 0)
        return -EINVAL;

    trace("fd1=%d, fd2=%d, flags=%x", fd1, fd2, flags);

    if (flags & ~O_CLOEXEC) return -EINVAL;
    if (fd1 == fd2) return -EINVAL;
    if (fd1 < 0 || fd1 >= NOFILE) return -EBADF;
    if (fd2 < 0 || fd2 >= NOFILE) return -EBADF;

    f = p->ofile[fd1];
    if (p->ofile[fd2])
        fileclose(p->ofile[fd2]);
    filedup(f);
    if (flags & O_CLOEXEC)
        bit_add(p->fdflag, fd2);
    p->ofile[fd2] = f;
    return fd2;
}


// ssize_t read(int fd, void *buf, size_t count);
long sys_read(void)
{
    struct file *f;
    int fd, n;
    uint64_t p;

    if (argu64(1, &p) < 0 || argint(2, &n) < 0)
        return -EINVAL;
    if (argfd(0, &fd, &f) < 0)
        return -EBADF;
    if (myproc()->pid == 8)
        trace("fd: %d, ip: %d, buf: 0x%lx, count: %d", fd, f->ip->inum, p, n);
    return fileread(f, p, n, 1);
}

// ssize_t pread64(int fd, void *buf, size_t count, off_t offset);
long sys_pread64(void)
{
    struct file *f;
    int fd;
    long offset, current, ret;
    uint64_t count, p;

    if (argu64(1, &p) < 0 || argu64(2, &count) < 0 || argu64(3, (uint64_t *)&offset))
        return -EINVAL;
    if (argfd(0, &fd, &f) < 0)
        return -EBADF;
    //if (myproc()->pid == 8)
        trace("fd: %d, ip: %d, buf: 0x%lx, count: %ld, offset: %ld", fd, f->ip->inum, p, count, offset);

    current = filelseek(f, 0, SEEK_CUR);
    filelseek(f, offset, SEEK_SET);
    ret = fileread(f, p, (int)count, 1);
    filelseek(f, current, SEEK_SET);
    return ret;
}

long sys_write(void)
{
    struct file *f;
    int n;
    uint64_t p;
    // int fd;

    if (argu64(1, &p) < 0 || argint(2, &n) < 0)
        return -EINVAL;
    //if (argfd(0, &fd, &f) < 0)
    if (argfd(0, 0, &f) < 0)
        return -EBADF;
#if 0
    if (fd == 1 || fd == 2) {
        char buf[n+1];
        copyin(myproc()->pagetable, buf, p, n);
        buf[n] = 0;
        debug("s = '%s'", buf);
    }
#endif
    trace("fd: %d, ip: %d, p: 0x%lx, n: %d", fd, f->ip->inum, p, n);
    return filewrite(f, p, n, 1);
}

struct iovec {
    void *iov_base;             /* Starting address. */
    size_t iov_len;             /* Number of bytes to transfer. */
};

// ssize_t readv(int d, const struct iovec *iov, int iovcnt);
long sys_readv(void)
{
    struct file *f;
    uint64_t iovp;
    int fd, iovcnt, i, bytes;
    struct iovec iov;
    ssize_t tot = 0;
    void *base;
    size_t len;

    if (argfd(0, &fd, &f) < 0) {
        error("fd: %d is not opened", fd);
        return -EBADF;
    }

    if (argu64(1, &iovp) < 0 || argint(2, &iovcnt) < 0)
        return -EINVAL;

    for (i = 0; i < iovcnt; i++) {
        if (copyin(myproc()->pagetable, (char *)&iov, iovp, sizeof(struct iovec)) < 0)
            return -EIO;
        base = iov.iov_base;
        len  = iov.iov_len;
        //if (myproc()->pid == 9)
        //    trace("base: %p, len: 0x%lx", base, len);
        bytes = fileread(f, (uint64_t)base, (int)len, 1);
        if (bytes < 0) {
            error("error: %d", bytes);
            return bytes;
        }
        tot += bytes;
        if (bytes == 0)
            break;
        iovp += sizeof(struct iovec);
    }

    //if (myproc()->pid == 9)
    //    trace("tot: %ld", tot);
    return tot;
}

// ssize_t writev(int d, const struct iovec *iov, int iovcnt);
ssize_t sys_writev(void)
{
    struct file *f;
    int fd, iovcnt;
    struct iovec *iov, *pp, iov_k;
    struct proc *p = myproc();
#if 0
    char buf[32];
    uint64_t blen;
    int i = 0;
#endif

    if (argu64(1, (uint64_t *)&iov) < 0 || argint(2, &iovcnt) < 0)
        return -EINVAL;
    if (iovcnt == 0)
        return 0;

    trace("n: 1, iov: %p", iov);
    if (argfd(0, &fd, &f) < 0)
        return -EBADF;
    trace("fd: %d, ip: %d, iov: %p, iovcnt: %d", fd, f->ip->inum, iov, iovcnt);
    ssize_t tot = 0;

    for (pp = iov; pp < iov + iovcnt; pp++) {
        if (copyin(p->pagetable, (char *)&iov_k, (uint64_t)pp, sizeof(struct iovec)) != 0)
            return -EIO;
#if 0
        if (fd == 1 || fd ==2) {
            blen = iov_k.iov_len > 32 ? 32 : iov_k.iov_len;
            copyin(p->pagetable, buf, (uint64_t)iov_k.iov_base, blen);
            debug("iov[%d] blen: %ld", i, blen)
            for (int j=0; j < blen; j++)
                printf(" %02x", buf[j]);
            printf("\n");
        }
#endif
        tot += filewrite(f, (uint64_t)iov_k.iov_base, iov_k.iov_len, 1);

        trace("[%d]: base: %p, len: 0x%lx, tot: 0x%lx", i++, iov_k.iov_base, iov_k.iov_len, tot);
    }
    return tot;
}

long sys_lseek()
{
    off_t offset;
    int whence;
    struct file *f;
    int fd;

    if (argfd(0, &fd, &f) < 0 || argu64(1, (uint64_t *)&offset) < 0
     || argint(2, &whence) < 0)
        return -EINVAL;

    if (whence & ~(SEEK_SET|SEEK_CUR|SEEK_END))
        return -EINVAL;

    trace("[%d] fd=%d, f.type=%d, offset=%lld, whence=%d",
        myproc()->pid, fd, f->type, offset, whence);

    if (f->type == FD_PIPE)
        return 0;

    return filelseek(f, offset, whence);
}


long sys_close(void)
{
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0)
        return -EBADF;

    if (myproc()->pid == 8)
        trace("pid[%d] fd: %d", myproc()->pid, fd);

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
    int ret;
    struct inode *ip;
    struct stat st;

    if (argint(0, &dirfd) < 0 || argu64(2, &staddr) < 0
     || argint(3, &flags) < 0)
        return -EINVAL;
    if ((ret = argstr(1, path, MAXPATH)) < 0)
        return -EFAULT;

    trace("dirfd: %d, path: %s, staddr: 0x%lx, flags: 0x%x", dirfd, path, staddr, flags);

    if (flags != 0 && (flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
        warn("unimplemented flags=0x%x", flags);
        return -EINVAL;
    }

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    begin_op();
loop:
    if ((ip = namei(path, dirfd)) == 0) {
        end_op();
        return -ENOENT;
    }
    ilock(ip);

    if ((flags & AT_SYMLINK_NOFOLLOW) == 0) {
        if (ip->type == T_SYMLINK) {
            int n;
            if ((n = readi(ip, 0, (uint64_t)path, 0, sizeof(MAXPATH) - 1)) <= 0) {
                warn("couldn't read sysmlink target");
                iunlockput(ip);
                end_op();
                return -ENOENT;
            }
            path[n] = 0;
            iunlockput(ip);
            goto loop;
        }
    }

    stati(ip, &st);
    iunlockput(ip);
    end_op();

    if (copyout(myproc()->pagetable, staddr, (char *)&st, sizeof(st)) < 0) {
        error("failed copyout");
        return -EFAULT;
    }

    return 0;
}

// int statx(int dirfd, const char *_Nullable restrict pathname,
//    int flags, unsigned int mask, struct statx *restrict statxbuf);
long sys_statx(void)
{
    char path[MAXPATH];
    uint64_t statxbufp; // user pointer to struct statx
    int dirfd, flags;
    unsigned int mask;

    struct proc *p = myproc();
    int ret;
    struct inode *ip;
    struct stat st;
    struct statx statx = { 0 };

    if (argint(0, &dirfd) < 0 || argint(2, &flags) < 0
     || argint(3, (int *)&mask) < 0 || argu64(4, &statxbufp) < 0)
        return -EINVAL;
    if ((ret = argstr(1, path, MAXPATH)) < 0)
        return -EFAULT;

    trace("dirfd: %d, path: %s, flags: 0x%x, mask: 0x%x, statx: 0x%lx", dirfd, path == NULL ? "null" : path, flags, mask, statxbufp);

    // AT_FDCWD, AT_EMPTY_PATH, AT_SYMLINK_NOFOLLOW, AT_STATX_SYNC_AS_STAT

    if (path == NULL || strlen(path) == 0) {
        if (!p->ofile[dirfd]) {
            error("dirfd %d is not opened", dirfd);
            return -EBADF;
        } else {
            ip = idup(p->ofile[dirfd]->ip);
            goto ip_lock;
        }
    } else {
        if ((ret = check_fdcwd(path, dirfd)) < 0)
            return ret;
    }

    begin_op();
loop:
    if ((ip = namei(path, dirfd)) == 0) {
        end_op();
        return -ENOENT;
    }
ip_lock:
    ilock(ip);

    if ((flags & AT_SYMLINK_NOFOLLOW) == 0) {
        if (ip->type == T_SYMLINK) {
            int n;
            if ((n = readi(ip, 0, (uint64_t)path, 0, sizeof(MAXPATH) - 1)) <= 0) {
                warn("couldn't read sysmlink target");
                iunlockput(ip);
                end_op();
                return -ENOENT;
            }
            path[n] = 0;
            iunlockput(ip);
            goto loop;
        }
    }

    stati(ip, &st);

    if (mask & STATX_TYPE) {
        statx.stx_mode = (st.st_mode & S_IFMT);
        statx.stx_mask |= STATX_TYPE;
    }
    if (mask & STATX_MODE) {
        statx.stx_mode = st.st_mode;
        statx.stx_mask |= STATX_MODE;
    }

    if (mask & STATX_NLINK) {
        statx.stx_nlink = st.st_nlink;
        statx.stx_mask |= STATX_NLINK;
    }

    if (mask & STATX_UID) {
        statx.stx_uid = st.st_uid;
        statx.stx_mask |= STATX_UID;
    }

    if (mask & STATX_GID) {
        statx.stx_gid = st.st_gid;
        statx.stx_mask |= STATX_GID;
    }

    if (mask & STATX_INO) {
        statx.stx_ino = st.st_ino;
        statx.stx_mask |= STATX_INO;
    }

    if (mask & STATX_SIZE) {
        if (S_ISLNK(st.st_mode) && (flags & AT_SYMLINK_NOFOLLOW))
            statx.stx_size = strlen(path);
        else
            statx.stx_size = st.st_size;
        statx.stx_mask |= STATX_SIZE;
    }

    if (mask & STATX_BLOCKS) {
        statx.stx_blksize = BSIZE;
        statx.stx_mask |= STATX_BLOCKS;
    }

    if (mask & STATX_ATIME) {
        statx.stx_atime.tv_sec  = st.st_atime.tv_sec;
        statx.stx_atime.tv_nsec = st.st_atime.tv_nsec;
        statx.stx_mask |= STATX_ATIME;
    }

    if (mask & STATX_MTIME) {
        statx.stx_mtime.tv_sec  = st.st_mtime.tv_sec;
        statx.stx_mtime.tv_nsec = st.st_mtime.tv_nsec;
        statx.stx_mask |= STATX_MTIME;
    }

    if (mask & STATX_CTIME) {
        statx.stx_ctime.tv_sec  = st.st_ctime.tv_sec;
        statx.stx_ctime.tv_nsec = st.st_ctime.tv_nsec;
        statx.stx_mask |= STATX_CTIME;
    }

    if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
        statx.stx_rdev_major = ip->major;
        statx.stx_rdev_minor = ip->minor;
    } else if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
        statx.stx_dev_major = SDMAJOR;
        statx.stx_dev_minor = XV6MINOR;
    }

    iunlockput(ip);
    end_op();

    if (copyout(p->pagetable, statxbufp, (char *)&statx, sizeof(statx)) < 0) {
        error("failed copyout");
        return -EFAULT;
    }

    if (myproc()->pid == 6)
        trace("path: %s, mask: 0x%x, stax mask: 0x%x, nlink: %d, uid: %d, gid: %d, mode: 0%03o, ino: %d, size: %ld, block: %ld", path == NULL ? "null" : path, mask,
        statx.stx_mask, statx.stx_nlink, statx.stx_uid, statx.stx_gid,
        statx.stx_mode, statx.stx_ino, statx.stx_size, statx.stx_blocks);
    return 0;
}

// Create the path new as a link to the same inode as old.
long sys_linkat(void)
{
    char newpath[MAXPATH], oldpath[MAXPATH];
    int olddirfd, newdirfd, flags;
    struct inode *ip;
    int ret;

    if (argint(0, &olddirfd) < 0 || argint(2, &newdirfd) < 0
     || argint(4, &flags) < 0
     || argstr(1, oldpath, MAXPATH) < 0
     || argstr(3, newpath, MAXPATH) < 0)
        return -EINVAL;

    trace("olddirfd: %d, oldpath: %s, newdirfd: %d, newpath: %s, flags: %d", olddirfd, oldpath, newdirfd, newpath, flags);

    if ((ret = check_fdcwd(oldpath, olddirfd)) < 0)
        return ret;

    if ((ret = check_fdcwd(newpath, newdirfd)) < 0)
        return ret;

    if (flags & AT_SYMLINK_FOLLOW) {
        begin_op();
        if ((ip = namei(oldpath, olddirfd)) == 0) {
            end_op();
            return -ENOENT;
        }
        ilock(ip);
        if (ip->type == T_SYMLINK) {
            int n;
            if ((n = readi(ip, 0, (uint64_t)oldpath, 0, sizeof(MAXPATH) - 1)) <= 0) {
                warn("couldn't follow symlink");
                iunlockput(ip);
                end_op();
                return -ENOENT;
            }
            oldpath[n] = 0;
        }
        iunlockput(ip);
        end_op();
    } else if (flags != 0) {
        return -EINVAL;
    }

    return filelink(oldpath, olddirfd, newpath, newdirfd);
}

long sys_symlinkat(void)
{
    char target[MAXPATH], linkpath[MAXPATH];
    int dirfd, ret;

    if (argstr(0, target, MAXPATH) < 0 || argint(1, &dirfd) < 0 || argstr(2, linkpath, MAXPATH) < 0)
        return -EINVAL;

    trace("dirfd: %d, target: %s, path: %s", dirfd, target, linkpath);

    if (strncmp(target, "", 1) == 0 || strncmp(linkpath, "", 1) == 0)
        return -ENOENT;

    if ((ret = check_fdcwd(linkpath, dirfd)) < 0)
        return ret;

    return filesymlink(target, linkpath, dirfd);
}

long sys_unlinkat(void)
{
    char path[MAXPATH];
    int dirfd, flags, ret;

    if (argint(0, &dirfd) < 0 || argint(2, &flags) < 0
     || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    trace("pid[%d] path: %s, fd: %d, flags: %d", myproc()->pid, path, dirfd, flags);

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    if (flags & ~AT_REMOVEDIR)
        return -EINVAL;

    return fileunlink(path, dirfd, flags);
}

// int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
ssize_t sys_readlinkat()
{
    char path[MAXPATH];
    uint64_t bufp;
    int dirfd;
    size_t bufsiz;
    int ret;

    if (argint(0, &dirfd) < 0 || argstr(1, path, MAXPATH) < 0
    || argu64(2, &bufp) < 0 || argu64(3, &bufsiz) < 0)
        return -EINVAL;

    //if ((ret = argptr(2, buf, bufsiz)) < 0)
    //    return -EFAULT;

    if ((ssize_t)bufsiz <= 0)
        return -EINVAL;

    trace("dirfd=%d, path=%s, bufp=0x%lx, bufsiz=%lld", dirfd, path, bufp, bufsiz);

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    if (!strncmp(path, "/proc/self/fd/", 14)) {   // TODO: ttyをちゃんと実装
        int fd = path[14] - '0';                  // /proc/self/fd/n -> /dev/tty/n, /dev/pts/n
        if (fd < 3) {
            copyout(myproc()->pagetable, bufp, "/dev/tty", 8);
            trace("return /dev/tty");
            return 8;
        }
    }

#if 0
    ssize_t lsize = filereadlink(path, dirfd, bufp, bufsiz);
    if (lsize > 0) {
        char link[lsize + 1];
        copyin(myproc()->pagetable, link, bufp, lsize);
        link[lsize] = 0;
        debug("buf: %s, bufsiz: %ld", link, lsize);
    } else {
        debug("error: %ld", lsize);
    }

    return lsize;
#endif

    return filereadlink(path, dirfd, bufp, bufsiz);
}

long sys_openat(void)
{
    char path[MAXPATH];
    int dirfd, flags, ret;
    mode_t mode;

    if (argint(0, &dirfd) < 0 || argint(2, &flags) < 0
     || argint(3, (int *)&mode) < 0
     || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -EINVAL;
    }

    if (myproc()->pid == 8)
        trace("dirfd: %d, path: '%s', mode: 0x%08x, flags: 0x%08x", dirfd, path, mode, flags);

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

/*
    if ((flags & O_LARGEFILE) == 0) {
        error("expect O_LARGEFILE in open flags");
        return -EINVAL;
    }
*/

    return fileopen(path, dirfd, flags, mode);
}

long sys_mkdirat(void)
{
    int dirfd, ret;
    char path[MAXPATH];
    mode_t mode;
    struct inode *ip;

    if (argint(0, &dirfd) < 0 || argint(2, (int *)&mode) < 0
     || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    trace("dirfd %d, path '%s', mode 0x%x", dirfd, path, mode);

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    begin_op();
    if ((long)(ip = create(path, dirfd, T_DIR, 0, 0, mode | S_IFDIR)) < 0) {
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
    int dirfd, ret;
    mode_t mode;
    dev_t  dev;
    uint16_t major=0, minor=0, type;

    if (argint(0, &dirfd) < 0 || argint(2, (int *)&mode) < 0
     || argu64(3, &dev) < 0 || argstr(1, path, MAXPATH) < 0)
        return -EINVAL;

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    begin_op();
    if (S_ISDIR(mode))
        type = T_DIR;
    else if (S_ISREG(mode))
        type = T_FILE;
    else if (S_ISCHR(mode) || S_ISBLK(mode))
        type = T_DEVICE;
    else if (S_ISLNK(mode))
        type = T_SYMLINK;

    else {
        warn("%d is not supported yet", mode & S_IFMT);
        return -EINVAL;
    }
    trace("path '%s', mode 0x%x, dev 0x%lx, type: %d", path, mode, dev, type);
    if (type == T_DEVICE) {
        major = (uint16_t)((dev >> 8) & 0xFF);
        minor = (uint16_t)(dev & 0xFF);
        trace("dev: 0x%016lx, major: %d, minor: %d", dev, major, minor);
    }

    trace("path: %s, mode: 0x%x, dev: 0x%lx, type: %d, major: %d, minor: %d", path, mode, dev, type, major, minor);

    if ((long)(ip = create(path, dirfd, type, major, minor, mode)) < 0) {
        end_op();
        error("error: 0x%lx", (long)ip);
        return (long)ip;
    }
    trace("created: ip=%d", ip->inum);
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
    if ((ip = namei(path, AT_FDCWD)) == 0) {
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
    char path[MAXPATH];
    char *argv[MAXARG], *envp[MAXARG];
    uint64_t argvp, envpp;
    uint64_t arg_p, env_p;
    int argc = 0, envc = 0, error;
    int i, ret;
    char buf[MAXPATH];

    if (argstr(0, path, MAXPATH) < 0) {
        warn("parse path error");
        return -EINVAL;
    }

    if (argu64(1, &argvp) < 0 || argu64(2, &envpp) < 0) {
        error("wrong argv or envp");
        return -EINVAL;
    }

    //if (p->pid <= 2)
        trace("path: '%s', argv: 0x%lx, envp: 0x%lx", path, argvp, envpp);

    memset(argv, 0, sizeof(argv));
    if (argvp != 0 && argvp != -1) {
        for (; ; argc++) {
            if (argc >= MAXARG - 1) {
                error("too many options: %d", argc);
                error = -E2BIG;
                goto bad;
            }
            if (fetchaddr(argvp + sizeof(uint64_t) * argc, (uint64_t *)&arg_p) < 0) {
                error("fetchaddr arg_p[%d]", argc);
                error = -EFAULT;
                goto bad;
            }
            if (arg_p == 0) {
                argv[argc] = 0;
                break;
            }
            if ((ret = fetchstr(arg_p, buf, MAXPATH)) < 0) {
                error("fetchstr error: argv[%d]", argc);
                error = -EIO;
                goto bad;
            }
            argv[argc] = kmalloc(ret + 1);
            if (argv[argc] == 0) {
                error("outof memory");
                error = -ENOMEM;
                goto bad;
            }
            memcpy(argv[argc], buf, ret+1);

#if 0
            //if (p->pid <= 2) {
                debug("argv[%d] (0x%lx) = %s", argc, argv[argc], argv[argc]);
                //for (int j=0; j < strlen(argv[argc]); j++) {
                //    printf("%02x ", argv[argc][j]);
                //}
                //printf("\n");
            //}
#endif
        }
    } else {
        argc = 0;
    }

    memset(envp, 0, sizeof(envp));
    if (envpp != 0 && envpp != -1) {
        for (; ; envc++) {
            if (envc >= MAXARG - 1) {
                error("too many environ: %d", envc);
                error = -E2BIG;
                goto bad;
            }
            if (fetchaddr(envpp + sizeof(uint64_t) * envc, (uint64_t*)&env_p) < 0) {
                error("fetchaddr error: env_p[%d]", envc);
                error = -EFAULT;
                goto bad;
            }
            if (env_p == 0) {
                envp[envc] = 0;
                break;
            }
            if ((ret = fetchstr(env_p, buf, MAXPATH)) < 0) {
                error("fetchstr error: env_p[%d]", envc);
                error = -EIO;
                goto bad;
            }

            envp[envc] = kmalloc(ret + 1);
            if (envp[envc] == 0) {
                error("outof memory");
                error = -ENOMEM;
                goto bad;
            }
            memcpy(envp[envc], buf, ret+1);
#if 0
            //if (p->pid == 3) {
                debug("envp[%d] (0x%lx) = %s", envc, envp[envc], envp[envc]);
                //for (int j=0; j < strlen(envp[envc]); j++) {
                //    printf("%02x ", envp[envc][j]);
                //}
                //printf("\n");
            //}
#endif
        }
    } else {
        envc = 0;
    }

    ret = execve(path, argv, envp, argc, envc);

    for (i = 0; i < argc; i++) {
        trace("free argv[%d]: %s", i, argv[i]);
        kmfree(argv[i]);
    }

    for (i = 0; i < envc; i++) {
        trace("free envp[%d]: %s", i, envp[i]);
        kmfree(envp[i]);
    }

    return ret;

bad:
    for (i = 0; i < argc; i++) {
        trace("bad: free argv[%d]: %s", i, argv[i]);
        kmfree(argv[i]);
    }

    for (i = 0; i < envc; i++) {
        trace("bad: free envp[%d]: %s", i, envp[i]);
        kmfree(envp[i]);
    }

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

    trace("pid[%d] fd: %d, f.type: %d, f.major: %d, req: 0x%lx, argp: %p", myproc()->pid, fd, f->type, f->major, req, argp);

    if (f->type != FD_INODE && f->ip->type != T_DEVICE) {
        warn("bad type: %d, %d", f->type, f->ip->type);
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
    uint64_t dirp;
    uint64_t count;
    struct file *f;

    if (argfd(0, &fd, &f) < 0) {
        if (f == 0)
            return -ENOENT;
        else
            return -EINVAL;
    }

    if (argu64(1, &dirp) < 0 || argu64(2, &count) <0)
        return -EINVAL;

    if (f->ip->type == T_DEVICE) {
        trace("fd: %d, type: %d", fd, f->ip->type);
        return -EBADF;
    } else if (f->ip->type != T_DIR) {
        trace("fd: %d, type: %d", fd, f->ip->type);
        return -ENOTDIR;
    }

    return getdents64(f, dirp, count);
}

/* カレントワーキングディレクトリ名の取得 */
void *sys_getcwd()
{
    uint64_t bufp;
    size_t size;
    char buf[MAXPATH];
    struct proc *p = myproc();
    struct inode *cwd, *dp;
    struct dirent de;
    int i, n, pos = 0;

    if (argu64(0, &bufp) < 0 || argu64(1, &size) < 0)
        return (void *)-EINVAL;

    begin_op();
    cwd = idup(p->cwd);
    if (cwd->inum == ROOTINO) goto root;
    while (1) {
        dp = dirlookup(cwd, "..", 0);
        ilock(dp);
        if (direntlookup(dp, cwd->inum, &de, 0) < 0)
            goto bad;
        n = strlen(de.name);
        if ((n + pos + 2) > size)
            goto bad;

        iput(cwd);
        iunlock(dp);
        for (i = 0; i < n; i++) {
            buf[pos + i] = de.name[n - i - 1];  // bufに逆順に詰める
        }
        pos += i;
        if (dp->inum == ROOTINO) break;
        buf[pos++] = '/';
        cwd = idup(dp);
        iput(dp);
    }
    iput(dp);
    end_op();

root:
    buf[pos] = '/';
    // bufを反転する
    char c;
    for (i = 0; i < (pos + 1) / 2; i++) {
        c = buf[i];
        buf[i] = buf[pos - i];
        buf[pos - i] = c;
    }
    buf[++pos] = 0;             // NULL終端
    trace("buf: %s", buf);
    if (bufp) {
        copyout(p->pagetable, bufp, buf, pos+1);
    }
    return (char *)bufp;

bad:
    iput(cwd);
    iunlockput(dp);
    end_op();
    return NULL;
}

long sys_sendfile(void)
{
    struct file *out_f, *in_f;
    uint64_t offset;
    size_t count;

    if (argfd(0, 0, &out_f) < 0 || argfd(1, 0, &in_f) < 0)
        return -EBADF;

    if (argu64(2, &offset) < 0)
        return -EFAULT;

    if (argu64(3, &count) < 0)
        return -EINVAL;

    return sendfile(out_f, in_f, offset, count);
}

long sys_fadvise64()
{
    int fd;
    off_t offset;
    off_t len;
    int advice;

    if (argfd(0, &fd, 0) < 0 || argu64(1, (uint64_t *)&offset) < 0
     || argu64(2, (uint64_t *)&len) < 0 || argint(3, &advice) < 0)
        return -EINVAL;

    if (advice < POSIX_FADV_NORMAL && advice > POSIX_FADV_NOREUSE)
        return -EINVAL;

    trace("fd=%d, offset=%d, len=%d, advice=0x%x", fd, offset, len, advice);

    return 0;
}

long sys_fchmodat()
{
    int dirfd, flags, ret;
    char path[MAXPATH];
    mode_t mode;

    if (argint(0, &dirfd) < 0 || argstr(1, path, MAXPATH) < 0
     || argint(2, (int *)&mode) < 0 || argint(3, &flags) < 0)
        return -EINVAL;

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    return filechmod(path, dirfd, mode);
}

long sys_fchown()
{
    int fd;
    uid_t owner;
    gid_t group;
    struct file *f;

    if (argfd(0, &fd, &f) < 0 || argint(1, (int *)&owner) < 0
     || argint(2, (int *)&group) < 0)
        return -EINVAL;

    return filechown(f, 0, AT_FDCWD, owner, group, 0);
}

long sys_fchownat()
{
    int dirfd, flags, ret;
    char path[MAXPATH];
    uid_t owner;
    gid_t group;

    if (argint(0, &dirfd) < 0 || argstr(1, path, MAXPATH) < 0
     || argint(2, (int *)&owner) < 0 || argint(3, (int *)&group) < 0
     || argint(4, &flags) < 0)
        return -EINVAL;

    trace("dirfd=%d, path=%s, uid=%d, gid=%d, flags=%d\n", dirfd, path, owner, group, flags);

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    return filechown(0, path, dirfd, owner, group, flags);
}

// int faccessat(int dirfd, const char *pathname, int mode, int flags);
long sys_faccessat(void)
{
    int dirfd, mode, flags, ret;
    char path[MAXPATH];

    if (argint(0, &dirfd) < 0 || argstr(1, path, MAXPATH) < 0
     || argint(2, &mode) < 0  || argint(3, &flags) < 0)
        return -EINVAL;

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    return faccess(path, dirfd, mode, flags);
}

// int faccessat2(int dirfd, const char *pathname, int mode, int flags);
long sys_faccessat2(void)
{
    int dirfd, mode, flags, ret;
    char path[MAXPATH];

    if (argint(0, &dirfd) < 0 || argstr(1, path, MAXPATH) < 0
     || argint(2, &mode) < 0  || argint(3, &flags) < 0)
        return -EINVAL;

    if ((ret = check_fdcwd(path, dirfd)) < 0)
        return ret;

    return faccess(path, dirfd, mode, flags);
}

long sys_llistxattr(void)
{
    char path[MAXPATH];
    uint64_t listp;
    size_t size;

    if (argstr(0, path, MAXPATH) < 0 || argu64(1, &listp) < 0
     || argu64(2, &size) < 0)
        return -EINVAL;

    // 拡張属性は実装しない
    return 0;
}

//int utimensat(int dirfd, const char *pathname,
//              const struct timespec times[2], int flags);
long sys_utimensat(void)
{
    int dirfd, flags;
    char path[MAXPATH];
    uint64_t timesp;
    struct timespec times[2];
    int pathlen, ret;
    struct file *f;

    struct inode *ip, *dp;
    char name[DIRSIZ];
    uint32_t off;
    struct timespec ts;

    if (argint(0, &dirfd) < 0 || (pathlen = argstr(1, path, MAXPATH)) < 0
    || argu64(2, &timesp) < 0 || argint(3, &flags) < 0)
        return -EINVAL;

    if (timesp != 0) {
        if (copyin(myproc()->pagetable, (char *)&times, timesp, sizeof(times)) < 0)
            return -EFAULT;
    } else {
        times[0].tv_nsec = UTIME_NOW;
        times[1].tv_nsec = UTIME_NOW;
    }

    trace("dirfd: %d, path: %s, times[0]: (0x%lx, 0x%lx), times[1]: (0x%lx, 0x%lx), flags: 0x%x", dirfd, path, times[0].tv_sec, times[0].tv_nsec, times[1].tv_sec, times[1].tv_nsec, flags);

    if ((flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) != 0) return -EINVAL;

    // pathが""の場合
    begin_op();
    if (pathlen == 0) {
        if (argfd(0, 0, &f) < 0)
            return -EINVAL;
        ip = f->ip;
    } else {
        if ((ret = check_fdcwd(path, dirfd)) < 0)
            return ret;
        if ((dp = nameiparent(path, name, dirfd)) == 0) {
            end_op();
            return -ENOENT;
        }

        ilock(dp);
        if ((ip = dirlookup(dp, name, &off)) == 0) {
            iunlockput(dp);
            end_op();
            return -ENOENT;
        }
        iunlockput(dp);
    }

    ilock(ip);
    for (int i = 0; i < 2; i++) {
        if (times[i].tv_nsec == UTIME_NOW) {
            clock_gettime(0, CLOCK_REALTIME, &ts);
            if (i == 0) ip->atime = ts;
            else        ip->mtime = ts;
        } else if (times[i].tv_nsec == UTIME_OMIT) {
            // noop
        } else {
            if (i == 0) ip->atime = times[i];
            else        ip->mtime = times[i];
        }
    }
    iupdate(ip);
    iunlockput(ip);
    end_op();

    return 0;
}

// int renameat2(int olddirfd, const char *oldpath,
//               int newdirfd, const char *newpath, unsigned int flags);
long sys_renameat2(void)
{
    int olddirfd, newdirfd, ret;
    char oldpath[MAXPATH], newpath[MAXPATH];
    uint32_t flags;

    if (argint(0, &olddirfd) < 0 || argstr(1, oldpath, MAXPATH) < 0
     || argint(2, &newdirfd) < 0 || argstr(3, newpath, MAXPATH) < 0
     || argint(4, (int *)&flags) < 0)
        return -EINVAL;

    if ((ret = check_fdcwd(oldpath, olddirfd)) < 0)
        return ret;
    if ((ret = check_fdcwd(newpath, newdirfd)) < 0)
        return ret;

    // TODO: RENAME_EXCHANGEの実装
    if (flags & RENAME_EXCHANGE)
        return -EINVAL;

    if (strncmp(oldpath, newpath, strlen(oldpath)) == 0) return 0;

    return filerename(oldpath, olddirfd, newpath, newdirfd, flags);
}

//ssize_t copy_file_range(int fd_in, off_t *_Nullable off_in,
//  int fd_out, off_t *_Nullable off_out, size_t size, unsigned int flags);
long sys_copy_file_range(void)
{
    return -EOPNOTSUPP;

#if 0
    int fd_in, fd_out, flags;
    off_t off_in, off_out;
    size_t size;
    struct file *f_in, *f_out;


    if (argfd(0, &fd_in, &f_in) < 0 || || argfd(2, &fd_out, &f_out) < 0)
        return -EBADF;

    if (argu64(1, &off_in) < 0 || argu64(3, &off_out) < 0
     || argu64(4, &size) < 0 || argint(5, &flags) < 0)
        return -EINVAL;

    if (!f_in->readable || !f_out->writable)
        return -EBADF;

    if (f_out->flags & O_APPEND)
        return -EBADF;

    if (off_out >= MAXFILE * BSIZE || off_out + size >= MAXFILE * BSIZE)
        return -EFBIG;

    if (flags != 0) return -EINVAL;

    if (fd_in == fd_out && off_in + size < off_out)
        return -EINVAL;

    if (f_in->ip->type == T_DIR || f_out->ip->type == T_DIR)
        return -EISDIR;
#endif
}
