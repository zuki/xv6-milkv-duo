#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include "mkfs.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE / (BSIZE * 8) + 1;     // 1500 / 32768 + 1 = 1
int ninodeblocks = (NINODES / IPB) + ((NINODES % IPB) == 0 ? 0 : 1);   // 1024 / 32 =  32)
int nlog = LOGSIZE;                     // 4
int nmeta;    // データブロック以外のブロック数
int nblocks;  // データブロック数

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;


void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type, uid_t uid, gid_t gid, mode_t mode);
void iappend(uint inum, void *p, int n);
void die(const char *);
uint make_dir(uint parent, char *name, uid_t uid, gid_t gid, mode_t mode);
uint make_dev(uint parent, char *name, int major, int minor, uid_t uid, gid_t gid, mode_t mode);
uint make_file(uint parent, char *name, uid_t uid, gid_t gid, mode_t mode);
void make_dirent(uint inum, ushort type, uint parent, char *name);
void copy_file(int start, int argc, char *files[], uint parent, uid_t uid, gid_t gid, mode_t mode);

// convert to riscv byte order
ushort xshort(ushort x)
{
    ushort y;
    uchar *a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint xint(uint x)
{
    uint y;
    uchar *a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

ulong xlong(ulong x)
{
    ulong y;
    uchar *a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    a[4] = x >> 32;
    a[5] = x >> 40;
    a[6] = x >> 48;
    a[7] = x >> 56;
    return y;
}


int
main(int argc, char *argv[])
{
    int i;
    uint rootino, off;
    char buf[BSIZE];
    struct dinode din;

    static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs fs.img files...\n");
        exit(1);
    }

    assert((BSIZE % sizeof(struct dinode)) == 0);
    assert((BSIZE % sizeof(struct dirent)) == 0);

    fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (fsfd < 0)
        die(argv[1]);

    // 1 fs block = 2 disk sector
    nmeta = 1 + 1 + nlog + ninodeblocks + nbitmap;  // 1 + 1 = root + superblock
    nblocks = FSSIZE - nmeta;

    sb.magic = FSMAGIC;
    sb.size = xint(FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(NINODES);
    sb.nlog = xint(nlog);

    sb.logstart = xint(2);
    sb.inodestart = xint(sb.logstart+nlog);
    sb.bmapstart = xint(sb.logstart+nlog+ninodeblocks);

    printf("SUPER-BLOCK: magic: 0x%08x, size: %d, nblks: %d, ninode: %d, nlog: %d\n",
        sb.magic, sb.size, sb.nblocks, sb.ninodes, sb.nlog);
    printf("             logstart: %d, inodestart: %d, bmapstart: %d\n",
        sb.logstart, sb.inodestart, sb.bmapstart);
    printf("FSSIZE: %d, nmeta: %d\n", FSSIZE, nmeta);

    freeblock = nmeta;     // the first free block that we can allocate

    for(i = 0; i < FSSIZE; i++)
        wsect(i, zeroes);

    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    rootino = ialloc(T_DIR, 0, 0, (S_IFDIR | 0775));
    assert(rootino == ROOTINO);
    make_dirent(rootino, T_DIR, rootino, ".");
    make_dirent(rootino, T_DIR, rootino, "..");

    copy_file(2, argc, argv, rootino, 0, 0, (S_IFREG | 0755));
#if 0
    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));


    for (i = 2; i < argc; i++) {
        // get rid of "user/"
        char *shortname;
        if (strncmp(argv[i], "obj/usr/bin/", 12) == 0)
            shortname = argv[i] + 12;
        else
            shortname = argv[i];

        assert(index(shortname, '/') == 0);

        if((fd = open(argv[i], 0)) < 0)
            die(argv[i]);

        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if (shortname[0] == '_')
            shortname += 1;

        inum = ialloc(T_FILE);

        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, shortname, DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        while((cc = read(fd, buf, sizeof(buf))) > 0)
            iappend(inum, buf, cc);

        close(fd);
    }
#endif

    // fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off/BSIZE) + 1) * BSIZE;
    din.size = xint(off);
    winode(rootino, &din);

    balloc(freeblock);

    exit(0);
}

// ディレクトリエントリの作成
void make_dirent(uint inum, ushort type, uint parent, char *name)
{
    struct dirent de;

    bzero(&de, sizeof(de));
    de.inum = xint(inum);
    //de.type = xshort(type);
    strncpy(de.name, name, DIRSIZ);
    //printf("DIRENT: inum=%d, name='%s' to PARNET[%d]\n", de.inum, de.name, parent);
    iappend(parent, &de, sizeof(de));
}

// ディレクトリの作成
uint make_dir(uint parent, char *name, uid_t uid, gid_t gid, mode_t mode)
{
    // Create parent/name
    uint inum = ialloc(T_DIR, uid, gid, mode);
    make_dirent(inum, T_DIR, parent, name);

    // Create parent/name/.
    make_dirent(inum, T_DIR, inum, ".");

    // Create parent/name/..
    make_dirent(parent, T_DIR, inum, "..");

    return inum;
}

// デバイスファイルの作成
uint make_dev(uint parent, char *name, int major, int minor, uid_t uid, gid_t gid, mode_t mode)
{
    struct dinode din;

    uint inum = ialloc(T_DEVICE, uid, gid, mode);
    make_dirent(inum, T_DEVICE, parent, name);
    rinode(inum, &din);
    din.major = xshort(major);
    din.minor = xshort(minor);
    winode(inum, &din);
    return inum;
}

// ファイルの作成
uint make_file(uint parent, char *name, uid_t uid, gid_t gid, mode_t mode)
{
    uint inum = ialloc(T_FILE, uid, gid, mode);
    make_dirent(inum, T_FILE, parent, name);
    return inum;
}

// 複数ファイルの作成
void copy_file(int start, int argc, char *argv[], uint parent, uid_t uid, gid_t gid, mode_t mode)
{
    int fd, cc;
    uint inum;
    char buf[BSIZE];

    for (int i = start; i < argc; i++) {
        char *shortname;
        if (strncmp(argv[i], "obj/usr/bin/", 12) == 0)
            shortname = argv[i] + 12;
        else
            shortname = argv[i];

        assert(index(shortname, '/') == 0);

        if((fd = open(argv[i], 0)) < 0)
            die(argv[i]);

        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if (shortname[0] == '_')
            shortname += 1;

        inum = make_file(parent, shortname, uid, gid, mode);
        while ((cc = read(fd, buf, sizeof(buf))) > 0)
            iappend(inum, buf, cc);
        close(fd);
    }
}

void
wsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
        die("lseek");
    if (write(fsfd, buf, BSIZE) != BSIZE)
        die("write");
}

void
winode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct dinode*)buf) + (inum % IPB);
    *dip = *ip;
    wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = inum / IPB + sb.inodestart;
    rsect(bn, buf);
    dip = ((struct dinode*)buf) + (inum % IPB);
    *ip = *dip;
}

void
rsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
        die("lseek");
    if (read(fsfd, buf, BSIZE) != BSIZE)
        die("read");
}

uint
ialloc(ushort type, uid_t uid, gid_t gid, mode_t mode)
{
    uint inum = freeinode++;
    struct dinode din;
    struct timespec ts1, ts2;

    clock_gettime(CLOCK_REALTIME, &ts1);
    ts2.tv_sec = xlong(ts1.tv_sec);
    ts2.tv_nsec = xlong(ts1.tv_nsec);
    //printf("inum[%d] ts: sec %ld, nsec %ld\n", inum, ts2.tv_sec, ts2.tv_nsec);
    bzero(&din, sizeof(din));
    din.type  = xshort(type);
    din.nlink = xshort(1);
    din.size  = xint(0);
    din.mode  = xint(mode);
    din.uid   = xint(uid);
    din.gid   = xint(gid);
    din.atime = din.mtime = din.ctime = ts2;
    winode(inum, &din);
    return inum;
}

void
balloc(int used)
{
    uchar buf[BSIZE];
    int i, j, k;

    //printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < nbitmap * BSIZE * 8);

    int used_blk = (used - 1) / (BSIZE * 8) + 1;

    for (j = 0; j < used_blk; j++) {
        bzero(buf, BSIZE);
        k = min(BSIZE * 8, used - j * BSIZE * 8);
        for (i = 0; i < k; i++) {
            buf[i/8] = buf[i/8] | (0x1 << (i % 8));
        }
        wsect(sb.bmapstart + j, buf);
    }
}

void
iappend(uint inum, void *xp, int n)
{
    char *p = (char*)xp;
    uint fbn, off, n1;
    struct dinode din;
    char buf[BSIZE];
    uint indirect[NINDIRECT];
    uint indirect2[NINDIRECT];
    uint x, idx1, idx2;

    rinode(inum, &din);
    off = xint(din.size);
    //printf("append inum %d at off %d sz %d, fbn=%d\n", inum, off, n, off/BSIZE);
    while (n > 0){
        fbn = off / BSIZE;
        assert(fbn < MAXFILE);
        if (fbn < NDIRECT) {
            if (xint(din.addrs[fbn]) == 0) {
                din.addrs[fbn] = xint(freeblock++);
            }
            x = xint(din.addrs[fbn]);
        } else if (fbn < (NDIRECT + NINDIRECT)) {
            if (xint(din.addrs[NDIRECT]) == 0) {
                din.addrs[NDIRECT] = xint(freeblock++);
            }
            rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            idx1 = fbn - NDIRECT;
            if (indirect[idx1] == 0) {
                indirect[idx1] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            }
            x = xint(indirect[idx1]);
        } else if (fbn < (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)) {
            if (xint(din.addrs[NDIRECT + 1]) == 0) {
                din.addrs[NDIRECT + 1] = xint(freeblock++);
            }
            rsect(xint(din.addrs[NDIRECT + 1]), (char *)indirect);
            idx1 = (fbn - NDIRECT - NINDIRECT) / NINDIRECT;
            idx2 = (fbn - NDIRECT - NINDIRECT) % NINDIRECT;
            if (xint(indirect[idx1]) == 0) {
                indirect[idx1] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT+1]), (char *)indirect);
            }
            rsect(xint(indirect[idx1]), (char *)indirect2);
            if (indirect2[idx2] == 0) {
                indirect2[idx2] = xint(freeblock++);
                wsect(xint(indirect[idx1]), (char *)indirect2);
            }
            x = xint(indirect2[idx2]);
        } else {
            printf("file is too big: fbr=%d\n", fbn);
            exit(1);
        }
        n1 = min(n, (fbn + 1) * BSIZE - off);
        rsect(x, buf);
        bcopy(p, buf + off - (fbn * BSIZE), n1);
        wsect(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = xint(off);
    winode(inum, &din);
}

void
die(const char *s)
{
    perror(s);
    exit(1);
}
