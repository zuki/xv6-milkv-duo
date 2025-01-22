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
filewrite(struct file *f, uint64_t addr, int n)
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
            if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
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

// ioctl request with arg from/to f
int fileioctl(struct file *f, unsigned long request, void *argp)
{
    if (f->type != FD_DEVICE)
        return -EINVAL;

    if (f->major < 0 || f->major >= NDEV || !devsw[f->major].ioctl)
        return -EINVAL;

    // FIXME: argpはローカルアドレスがセットされているのでポインタとして
    // 使用する際は適切にcopyin()すること
    return devsw[f->major].ioctl(1, request, argp);
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

    if ((error = dirlink(dp, name, ip->inum)) != 0) {
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

long fileunlink(char *path, int flags)
{
    debug("path: %s, flags: %d", path, flags);
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
    ip->mode  = mode;
    ip->type  = type;
    rtc_gettime(&ts);
    ip->atime = ip->mtime = ip->ctime = ts;
    ip->uid = myproc()->uid;
    ip->gid = myproc()->gid;
    iupdate(ip);

    if (type == T_DIR) {  // Create . and .. entries.
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
        goto fail;
    }

    if (dirlink(dp, name, ip->inum) < 0)
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
    //char buf[512];
    int fd;
    //long error;
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
//loop:
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
/*
        if (ip->type == T_SYMLINK) {
            if ((n = readi(ip, buf, 0, sizeof(buf) - 1)) <= 0) {
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
*/
    }

    int readable = FILE_READABLE((int)flags);
    int writable = FILE_WRITABLE((int)flags);
/*
    if (readable && ((error = ip->iops->permission(ip, MAY_READ)) < 0))
        goto bad;
    if (writable && ((error = ip->iops->permission(ip, MAY_WRITE)) < 0))
        goto bad;
*/
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
/*
bad:
    iunlockput(ip);
    end_op();
    return -EACCES;
*/
}
