//
// Support functions for system calls that involve file descriptors.
//

#include <common/types.h>
#include <common/riscv.h>
#include <defs.h>
#include <common/param.h>
#include <common/fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <common/file.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/time.h>
#include <linux/capability.h>
#include <proc.h>
#include <errno.h>
#include <printf.h>

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dp)
{
    int off;
    struct dirent de;

    for (off=2*sizeof(de); off<dp->size; off+=sizeof(de)) {
        if (readi(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty: readi");
        if(de.inum != 0)
            return 0;
    }
    return 1;
}

// ファイル構造体を割り当てる.
struct file*
filealloc(void)
{
    struct file *f;

    acquire(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++) {
        if(f->ref == 0) {
            f->ref = 1;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;
}

// ファイルfのrefカウンタを増分.
struct file*
filedup(struct file *f)
{
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("filedup");
    f->ref++;
    release(&ftable.lock);
    return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0){
        release(&ftable.lock);
        return;
    }
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&ftable.lock);

    if (ff.type == FD_PIPE){
        pipeclose(ff.pipe, ff.writable);
    } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
        begin_op();
        iput(ff.ip);
        end_op();
    }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64_t addr)
{
    struct proc *p = myproc();
    struct stat st;

    if (f->type == FD_INODE || f->type == FD_DEVICE) {
        ilock(f->ip);
        stati(f->ip, &st);
        iunlock(f->ip);
    } else if (f->type == FD_PIPE) {
        st.st_mode = (S_IFCHR|S_IRUSR|S_IWUSR);
        st.st_size = 0;
        st.st_nlink = 1;
        st.st_blocks = 0;
        st.st_blksize = 4096;
    } else {
        return -EFAULT;
    }

    if (copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
        return -EFAULT;

    return 0;
}

// ファイルfからデータを読み込む.
// addrはユーザ空間の仮想アドレス
int fileread(struct file *f, uint64_t addr, int n, int user)
{
    int r = 0;

    if (f->readable == 0)
        return -1;

    if (f->type == FD_PIPE){
        r = piperead(f->pipe, addr, n);
    } else if(f->type == FD_DEVICE){
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
            return -EINVAL;
        r = devsw[f->major].read(1, addr, n);
    } else if(f->type == FD_INODE){
        ilock(f->ip);
        if ((r = readi(f->ip, user, addr, f->off, n)) > 0) {
            //debug("inum: %d, off: %d, read: %d", f->ip->inum, f->off, r);
            f->off += r;
        }
        rtc_gettime(&f->ip->atime);
        iunlock(f->ip);
    } else {
        panic("fileread");
    }

    return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64_t addr, int n, int user)
{
    trace("ip: %d, addr: 0x%lx, n: %d", f->ip->inum, addr, n);
    int r, ret = 0;
    struct timespec ts;

    if (f->writable == 0)
        return -1;

    if (f->type == FD_PIPE) {
        ret = pipewrite(f->pipe, addr, n);
    } else if(f->type == FD_DEVICE) {
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
            return -1;
        ret = devsw[f->major].write(1, addr, n);
    } else if(f->type == FD_INODE){
        // write a few blocks at a time to avoid exceeding
        // the maximum log transaction size, including
        // i-node, indirect block, allocation blocks,
        // and 2 blocks of slop for non-aligned writes.
        // this really belongs lower down, since writei()
        // might be writing a device like the console.
        ssize_t max = ((MAXOPBLOCKS-1-2-2) / 2) * BSIZE;
        ssize_t i = 0;
        while (i < n) {
            ssize_t n1 = MIN(max, n - i);
            begin_op();
            ilock(f->ip);
            if ((r = writei(f->ip, user, addr + i, f->off, n1)) > 0)
                f->off += r;
            rtc_gettime(&ts);
            f->ip->mtime = f->ip->atime = ts;
            iunlock(f->ip);
            end_op();

            if (r != n1) {
                // error from writei
                break;
            }
            i += r;
        }
        ret = (i == n ? n : -1);
    } else {
        panic("filewrite");
    }

    return ret;
}

long sendfile(struct file *out_f, struct file *in_f, off_t offsetp, size_t count)
{
    off_t offset;
    size_t n, bytes = 0;
    struct proc *p = myproc();
    long ret = 0;

    char *buf = kalloc();
    if (buf == NULL)
        return -ENOMEM;

    if (offsetp) {
        offset = in_f->off;
        copyin(p->pagetable, (char *)&in_f->off, offsetp, sizeof(off_t));
    }
    while(1) {
        n = count > PGSIZE ? PGSIZE : count;
        if (fileread(in_f, (uint64_t)buf, n, 0) != n) {
            ret = -EIO;
            goto out;
        }
        if (filewrite(out_f, (uint64_t)buf, n, 0) != n) {
            ret = -EIO;
            goto out;
        }
        count -= n;
        bytes += n;
        if (count <= 0)
            break;
    }
    if (offsetp) {
        copyout(p->pagetable, offsetp, (char *)&in_f->off, sizeof(off_t));
        in_f->off = offset;
    }

out:
    kfree(buf);

    return ret < 0 ? ret : bytes;
}

// 書き込みのあったMAP_SHAREのmmap領域の1ページをファイルに書き戻す
// 書き戻しはファイルのサイズ内部分のみ行う。サイズ外の変更は無視する。

int writeback(struct file *f, off_t off, uint64_t addr)
{
    int r;
    struct timespec ts;

    if (f == NULL || f->writable == 0 || f->type != FD_INODE) {
        return 0;
    }

    uint32_t n = off + PGSIZE > f->ip->size ? f->ip->size - off : PGSIZE;
    begin_op();
    ilock(f->ip);
    r = writei(f->ip, 1, addr, off, n);
    rtc_gettime(&ts);
    f->ip->mtime = f->ip->atime = ts;
    iunlock(f->ip);
    end_op();
    trace("addr: 0x%lx, off: 0x%lx, n: %d, r: %d", addr, off, n, r);
    return r == n ? r : -1;
}

// ioctl request with arg from/to f
int fileioctl(struct file *f, unsigned long request, void *argp)
{
    // request TCSBRK/TCFLSH はf->type = FD_INODE/FD_DEVICEで発生
    if (f->type == FD_INODE) {
        if (request == TCSBRK || request == TCFLSH) {
            // FIXME: drain, flushを実装すること
            return 0;
        }
    }

    if (f->type != FD_DEVICE) {
        error("inval1");
        return -EINVAL;
    }

    // TODO: devsw[SDMAJOR] を実装
    if (f->major < 0 || f->major >= NDEV || !devsw[f->major].ioctl) {
        error("inval2");
        return -EINVAL;
    }

    return devsw[f->major].ioctl(1, request, argp);
}

long filelseek(struct file *f, off_t offset, int whence)
{
    if (!f) return -EBADF;

    switch(whence) {
        case SEEK_SET:
            if (offset < 0)
                goto bad;
            else
                f->off = offset;
            break;
        case SEEK_CUR:
            if (f->off + offset < 0)
                goto bad;
            else
                f->off += offset;
            break;
        case SEEK_END:
            if (f->ip->size + offset < 0)
                goto bad;
            else
                f->off = f->ip->size + offset;
            break;
        default:
            goto bad;
    }
    return f->off;

bad:
    debug("invalid offset %d", offset)
    return -EINVAL;
}

long filelink(char *old, char *new)
{
    char name[DIRSIZ];
    struct inode *dp, *ip;
    long error;

    begin_op();
    if ((ip = namei(old)) == 0) {
        end_op();
        return -ENOENT;
    }

    ilock(ip);
    if (ip->type == T_DIR) {
        iunlockput(ip);
        end_op();
        return -EPERM;
    }

    ip->nlink++;
    iupdate(ip);
    iunlock(ip);

    if ((dp = nameiparent(new, name)) == 0) {
        error = -ENOENT;
        goto bad;
    }

    ilock(dp);
    if (dp->dev != ip->dev) {
        error = -EXDEV;
        iunlockput(dp);
        goto bad;
    }

    if ((error = dirlink(dp, name, ip->inum, ip->type)) != 0) {
        iunlockput(dp);
        goto bad;
    }

    iunlockput(dp);
    iput(ip);
    end_op();
    return 0;

bad:
    ilock(ip);
    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);
    end_op();
    return error;
}

long filesymlink(char *old, char *new)
{
    char name[DIRSIZ];
    struct inode *dp, *ip;
    long error;
    struct timespec ts;

    begin_op();
    if ((dp = nameiparent(new, name)) == 0) {
        end_op();
        return -ENOENT;
    }

    ilock(dp);
    if ((ip = ialloc(dp->dev, T_SYMLINK)) == 0) {
        iunlockput(dp);
        end_op();
        return -ENOMEM;
    }

    ilock(ip);
    ip->major = 0;
    ip->minor = 0;
    ip->nlink = 1;
    ip->mode = S_IFLNK | 0777;
    ip->type = T_SYMLINK;
    rtc_gettime(&ts);
    ip->atime = ip->mtime = ip->ctime = ts;
    iupdate(ip);

    if ((error = dirlink(dp, name, ip->inum, ip->type)) != 0) {
        iunlockput(dp);
        iunlockput(ip);
        end_op();
        return error;
    }

    iupdate(dp);
    iunlockput(dp);
    trace("old: %s, len: %d", old, strlen(old));
    writei(ip, 0, (uint64_t)old, 0, strlen(old));
    iupdate(ip);
    iunlockput(ip);
    end_op();
    return 0;
}

ssize_t filereadlink(char *path, char *buf, size_t bufsize)
{
    struct inode *ip, *dp;
    ssize_t n;
    char name[DIRSIZ];

    begin_op();
    if ((dp = nameiparent(path, name)) == 0) {
        end_op();
        return -ENOTDIR;
    } else {
        iput(dp);
    }

    if ((ip = namei(path)) == 0) {
        end_op();
        return -ENOENT;
    }

    ilock(ip);
    if (ip->type != T_SYMLINK) {
        iunlockput(ip);
        end_op();
        return -EINVAL;
    }

    if ((n = readi(ip, 1, (uint64_t)buf, 0, bufsize)) <= 0) {
        iunlockput(ip);
        end_op();
        return -EIO;
    }

    iunlockput(ip);
    end_op();
    return n;
}

long fileunlink(char *path, int flags)
{
    trace("path: %s, flags: %d", path, flags);
    struct inode *ip, *dp;
    char name[DIRSIZ];
    uint32_t off;
    uint32_t error;

    begin_op();
    if ((dp = nameiparent(path, name)) == 0) {
        end_op();
        return -ENOENT;
    }

    ilock(dp);

    /* Cannot unlink "." or "..". */

    if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
        error = -EPERM;
        goto baddp;
    }

    if ((ip = dirlookup(dp, name, &off)) == 0) {
        error = -ENOENT;
        goto baddp;
    }

    ilock(ip);

    if (ip->nlink < 1) {
        error = -EPERM;
        goto badip;
        //panic("unlink: nlink < 1");
    }

    if (flags & AT_REMOVEDIR) {
        if (ip->type != T_DIR) {
            error = -ENOTDIR;
            goto badip;
        }
        if (!isdirempty(ip)) {
            error = -EPERM;
            goto badip;
        }
    } else {
        if (ip->type == T_DIR) {
            error = -EISDIR;
            goto badip;
        }
    }

    if (unlink(dp, off) < 0) {
        error = -EIO;
        goto badip;
        //panic("unlink: unlink");
    }

    if (ip->type == T_DIR) {
        dp->nlink--;
        iupdate(dp);
    }
    iunlockput(dp);

    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);
    end_op();
    return 0;

badip:
    iunlockput(ip);
baddp:
    iunlockput(dp);
    end_op();
    return error;
}

struct inode *create(char *path, short type, short major, short minor, mode_t mode)
{
    struct inode *ip, *dp;
    char name[DIRSIZ];
    struct timespec ts;

    //debug("path: %s, type: %d, major: %d, minor: %s, mode: %x", path, type, major, minor, mode);

    if ((dp = nameiparent(path, name)) == 0)
        return (void *)-ENOENT;

    ilock(dp);

    if (permission(dp, MAY_EXEC) < 0) {
        iunlockput(dp);
        return (void *)-EACCES;
    }

    if ((ip = dirlookup(dp, name, 0)) != 0){
        iunlockput(dp);
        ilock(ip);
        trace("exit: name: %s, type: %d, ip:%d, ip.type: %d, ip.major=%d", name, type, ip->inum, ip->type, ip->major);
        if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE)) {
            return ip;
        }
        iunlockput(ip);
        return (void *)-EEXIST;
    }

    if ((ip = ialloc(dp->dev, type)) == 0){
        iunlockput(dp);
        return (void *)-ENOMEM;
    }

    ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    ip->mode  = mode & ~(myproc()->umask);
    ip->type  = type;
    rtc_gettime(&ts);
    ip->atime = ip->mtime = ip->ctime = ts;
    ip->uid = myproc()->uid;
    ip->gid = myproc()->gid;
    iupdate(ip);

    if (type == T_DIR) {  // Create . and .. entries.
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (dirlink(ip, ".", ip->inum, ip->type) < 0 || dirlink(ip, "..", dp->inum, dp->type) < 0)
        goto fail;
    }

    if (dirlink(dp, name, ip->inum, ip->type) < 0)
        goto fail;

    if (type == T_DIR){
        // now that success is guaranteed:
        dp->nlink++;  // for ".."
        iupdate(dp);
    }

    iunlockput(dp);
    trace("create: name: %s, type: %d, ip:%d, ip.type: %d, ip.major=%d", name, type, ip->inum, ip->type, ip->major);
    return ip;

fail:
    // something went wrong. de-allocate ip.
    ip->nlink = 0;
    iupdate(ip);
    iunlockput(ip);
    iunlockput(dp);
    return 0;
}

long fileopen(char *path, int flags, mode_t mode)
{
    struct inode *ip;
    struct file *f;
    char buf[512];
    int fd, n;
    trace("path: %s, flags: %d, mod: %ld", path, flags, mode);

    begin_op();
    if (flags & O_CREAT) {
        ip = namei(path);
        if (ip && flags & O_EXCL) {
            iput(ip);
            end_op();
            warn("O_CREAT && O_EXCL and  %s exists", path);
            return -EEXIST;
        } else if (!ip) {
            if ((ip = create(path, T_FILE, 0, 0, mode | S_IFREG)) == 0) {
                end_op();
                warn("cant create %s", path);
                return -EDQUOT;
            }
        } else {
            ilock(ip);
        }
    } else {
loop:
        if ((ip = namei(path)) == 0) {
            end_op();
            trace("%s is not found", path);
            return -ENOENT;
        }
        ilock(ip);
        trace("ip: num: %d, type: %d", ip->inum, ip->type);
        if (ip->type == T_DIR && (flags & O_ACCMODE) != 0) {
            iunlockput(ip);
            end_op();
            warn("wrong flags 0x%llx", flags);
            return -EINVAL;
        }

        if (ip->type == T_SYMLINK) {
            if ((n = readi(ip, 0, (uint64_t)buf, 0, sizeof(buf) - 1)) <= 0) {
                iunlockput(ip);
                end_op();
                warn("couldn't read sysmlink target");
                return -ENOENT;
            }
            buf[n] = 0;
            path = buf;
            iunlockput(ip);
            goto loop;
        }
    }

    int readable = FILE_READABLE((int)flags);
    int writable = FILE_WRITABLE((int)flags);

    if (readable && (permission(ip, MAY_READ) < 0))
        goto bad;
    if (writable && (permission(ip, MAY_WRITE) < 0))
        goto bad;

    if ((f = filealloc()) == 0 || (fd = fdalloc(f, 0)) < 0) {
        iunlock(ip);
        end_op();
        if (f) fileclose(f);
        warn("cant alloc file\n");
        return -ENOSPC;
    }

    if (ip->type == T_DEVICE) {
        f->type = FD_DEVICE;
        f->major = ip->major;
    } else {
        f->type     = FD_INODE;
        f->off      = (flags & O_APPEND) ? ip->size : 0;
        f->major    = 0;
    }
    f->ip       = ip;
    f->flags    = flags;
    f->readable = readable;
    f->writable = writable;
    if (flags & O_CLOEXEC)
        bit_add(myproc()->fdflag, fd);

    trace("inum: %d, fd: %d", ip->inum, fd);

    iunlock(ip);
    end_op();

    return fd;

bad:
    iunlockput(ip);
    end_op();
    return -EACCES;
}

long filechmod(char *path, mode_t mode)
{
    struct inode *ip;

    begin_op();
    if ((ip = namei(path)) == 0) {
        end_op();
        return -ENOENT;
    }

    ilock(ip);
    ip->mode = (ip->mode & S_IFMT) | mode;
    iupdate(ip);
    iunlockput(ip);
    end_op();

    return 0;
}

long filechown(struct file *f, char *path, uid_t owner, gid_t group)
{
    struct inode *ip;
    struct proc *p = myproc();
    long error = -EPERM;
    //int i;

    begin_op();
    if (f != NULL) {
        ip = f->ip;
    } else {
        if ((ip = namei(path)) == 0) {
            end_op();
            return -ENOENT;
        }
    }

    ilock(ip);

    if (owner != (uid_t)-1) {
        if (!capable(CAP_CHOWN)) {
            debug("uid %d cant chown", owner);
            goto bad;
        }
        ip->uid = owner;
    }

    if (group != (gid_t)-1) {
        if (capable(CAP_CHOWN)) {
            ip->gid = group;
        } else if (ip->uid == p->euid) {
            //for (i = 0; i < p->ngroups; i++) {
            //    if (p->groups[i] == group) {
                    ip->gid = group;
            //        break;
            //    }
            //}
            //if (i == p->ngroups) {
            //    debug("uid %d and gid %d cant chown", owner, group);
            //    goto bad;
            //}
        } else {
            debug("gid %d cant chown", group);
            goto bad;
        }
    }

    if (ip->mode & S_IXUGO && !capable(CAP_CHOWN)) {
        ip->mode &= ~(S_ISUID|S_ISGID);
    }
    iupdate(ip);
    error = 0;

bad:
    iunlockput(ip);
    end_op();
    return error;
}

struct file *fileget(char *path)
{
    struct inode *ip;
    struct file *f;
    char buf[512];
    int n;
    long error;
    trace("path: %s", path);
    begin_op();
loop:
    if ((ip = namei(path)) == 0) {
        end_op();
        return (void *)-ENOENT;
    }
    ilock(ip);

    if (ip->type == T_SYMLINK) {
        if ((n = readi(ip, 0, (uint64_t)buf, 0, sizeof(buf) - 1)) <= 0) {
            warn("couldn't read sysmlink target");
            error = -ENOENT;
            goto bad;
        }
        buf[n] = 0;
        path = buf;
        iunlockput(ip);
        goto loop;
    }

    if (ip->type != T_FILE) {
        error("inum: %d, type: %d", ip->inum, ip->type);
        error = -EINVAL;
        goto bad;
    }

    if ((f = filealloc()) == 0) {
        error = -ENOSPC;
        goto bad;
    }

    if ((error = (long)permission(ip, MAY_READ)) < 0) {
        error("permission error: %d", error);
        goto bad;
    }

    f->type     = FD_INODE;
    f->ref      = 1;
    f->ip       = ip;
    f->off      = 0;
    f->flags    = O_RDONLY;
    f->readable = 1;
    f->writable = 0;

    iunlock(ip);            // ここでputするとv6/ext2_inode部分がクリアされる
    end_op();               // putはfをcloseする際に行われる

    trace("ok: inum: %d, path: %s", ip->inum, path);
    return f;

bad:
    iunlockput(ip);
    end_op();
    return (void *)error;
}

long faccess(char *path, int mode, int flags)
{
    struct inode *ip;
    struct proc *p = myproc();
    uid_t uid, fuid;
    gid_t gid, fgid;
    mode_t fmode;

    if (mode & ~S_IRWXO)    /* where's F_OK, X_OK, W_OK, R_OK? */
        return -EINVAL;

    begin_op();
    if ((ip = namei(path)) == 0) {
        end_op();
        return -ENOENT;
    }
    fmode = ip->mode;
    fuid = ip->uid;
    fgid = ip->gid;
    iput(ip);
    end_op();

    // mode == F_OKの場合はファイルが存在するのでOK
    if (mode == 0) return 0;

    // 呼び出し元がrootならR_OK, W_OKは常にOK
    // X_OKはファイルにUGOのいずれかに実行許可があればOK
    if (p->uid == 0) {
        if ((mode & X_OK) && !(fmode & S_IXUGO))
            return -EACCES;
        else
            return 0;
    }

    // root以外は個別に判断
    if (flags & AT_EACCESS) {       // 実効IDで判断
        uid = p->euid;
        gid = p->egid;
    } else {                        // 実IDで判断
        uid = p->uid;
        gid = p->gid;
    }

    if (mode & R_OK) {
        if ((fuid == uid && !(fmode & S_IRUSR))
         && (fgid == gid && !(fmode & S_IRGRP))
                         && !(fmode & S_IROTH))
        return -EACCES;
    }

    if (mode & W_OK) {
        if ((fuid == uid && !(fmode & S_IWUSR))
         && (fgid == gid && !(fmode & S_IWGRP))
                         && !(fmode & S_IWOTH))
        return -EACCES;
    }

     if (mode & X_OK) {
        if ((fuid == uid && !(fmode & S_IXUSR))
         && (fgid == gid && !(fmode & S_IXOTH))
                         && !(fmode & S_IXOTH))
        return -EACCES;
    }

    return 0;
}

// dp/name1 -> dp/name2 へ改名
static long rename(struct inode *dp, char *name1, char *name2)
{
    struct inode *ip;
    struct dirent de;
    uint32_t off;

    ilock(dp);
    if ((ip = dirlookup(dp, name1, &off)) == 0)
        return -ENOENT;
    ilock(ip);

    memset(&de, 0, sizeof(de));
    de.inum = ip->inum;
    de.type = ip->type;
    memmove(de.name, name2, DIRSIZ);
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
        warn("writei");
        return -ENOSPC;
    }
    iupdate(dp);
    rtc_gettime(&ip->ctime);
    iupdate(ip);
    iunlockput(ip);
    iunlockput(dp);

    return 0;
}

// dp/old_ip -> dp/new_ip へ付け替え
static long reinode(struct inode *dp, struct inode *old_ip, struct inode *new_ip)
{
    struct dirent de;
    size_t off;
    struct timespec ts;

    ilock(dp);
    ilock(old_ip);
    ilock(new_ip);
    if (direntlookup(dp, old_ip->inum, &de, &off) < 0)
        return -ENOENT;

    de.inum = new_ip->inum;
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
        warn("writei");
        return -ENOSPC;
    }
    iupdate(dp);
    rtc_gettime(&ts);
    old_ip->ctime = new_ip->ctime = ts;
    iupdate(old_ip);
    iupdate(new_ip);
    iunlockput(old_ip);
    iunlockput(new_ip);
    iunlockput(dp);

    return 0;
}



long filerename(char *path1, char *path2, uint32_t flags)
{
    struct inode *ip1, *ip2, *dp1, *dp2;
    char name1[DIRSIZ], name2[DIRSIZ];
    long error;

    begin_op();
    if (flags & RENAME_NOREPLACE) {
        if ((ip2 = namei(path2)) != 0) {
            iunlock(ip2);
            end_op();
            return -EEXIST;
        }
    }

    if ((ip1 = namei(path1)) == 0) {
        end_op();
        warn("path1 %s not exits", path1);
        return -ENOENT;
    }
    dp1 = nameiparent(path1, name1);

    ip2 = namei(path2);
    dp2 = nameiparent(path2, name2);
    trace("path1: %s, dp1: %d, ip1: %d, name1: %s", path1, dp1->inum, ip1->inum, name1);
    trace("path2: %s, dp2: %d, ip2: %d, name2: %s", path2, dp2 ? dp2->inum : -1, ip2 ? ip2->inum : -1, name2);

    // 同一ファイルのhard link
    if (ip1 == ip2) {
        iput(ip1);
        iput(ip2);
        iput(dp1);
        iput(dp2);
        return 0;
    }

    error = -EINVAL;
    // 親ディレクトリが同じ
    if (dp1 == dp2) {
        // name2はなし: 単なる改名(name1はdirectoryでもfileでも可)
        if (ip2 == 0) {
            if ((error = rename(dp1, name1, name2)) < 0) {
                warn("rename failed");
                goto bad;
            }
            iput(ip1);
            iput(dp2);
        // name2あり: direentを付け替えて、ip2はunlink
        } else {
            if ((error = reinode(dp2, ip2, ip1)) < 0) {
                warn("reinode failed");
                goto bad;
            }
            iput(dp1);
            end_op();
            return fileunlink(path2, ip2->type == T_DIR ? AT_REMOVEDIR : 0);
        }
    // 異なるディレクトリへのmove
    } else {
        if (ip2 == 0) {
            if ((error = dirlink(dp2, name2, ip1->inum, ip1->type)) < 0) {
                warn("dirlink failed 2");
                goto bad;
            }
            flags = ip1->type == T_DIR ? AT_REMOVEDIR : 0;
            iput(dp1);
            iput(ip1);
            end_op();
            return fileunlink(path1, flags);
        } else {
            if ((error = reinode(dp2, ip2, ip1)) < 0) {
                warn("reinode failed 2");
                goto bad;
            }
            flags = ip2->type == T_DIR ? AT_REMOVEDIR : 0;
            iput(dp1);
            iput(ip2);
            end_op();
            return fileunlink(path2, flags);
        }
    }
    error = 0;

bad:
    end_op();

    return error;
}
