#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#include "common/types.h"
#include "common/fs.h"
#include "v6_stat.h"
#include "common/param.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#define NINODES 256

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE/(1024*8) + 1;     // 1500 / 8192 + 1 = 1
int ninodeblocks = (NINODES / IPB) + ((NINODES % IPB) == 0 ? 0 : 1);   // 256 / 16 = 1
int nlog = LOGSIZE;                     // 30
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
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
void die(const char *);

// convert to riscv byte order
ushort
xshort(ushort x)
{
    ushort y;
    uchar *a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint
xint(uint x)
{
    uint y;
    uchar *a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

int
main(int argc, char *argv[])
{
    int i, cc, fd;
    uint rootino, inum, off;
    struct dirent de;
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

    rootino = ialloc(T_DIR);
    assert(rootino == ROOTINO);

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));

    for(i = 2; i < argc; i++){
        // get rid of "user/"
        char *shortname;
        if(strncmp(argv[i], "user/", 5) == 0)
        shortname = argv[i] + 5;
        else
        shortname = argv[i];

        assert(index(shortname, '/') == 0);

        if((fd = open(argv[i], 0)) < 0)
        die(argv[i]);

        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if(shortname[0] == '_')
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

    // fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off/BSIZE) + 1) * BSIZE;
    din.size = xint(off);
    winode(rootino, &din);

    balloc(freeblock);

    exit(0);
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
    if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
        die("lseek");
    if(read(fsfd, buf, BSIZE) != BSIZE)
        die("read");
}

uint
ialloc(ushort type)
{
    uint inum = freeinode++;
    struct dinode din;

    bzero(&din, sizeof(din));
    din.type = xshort(type);
    din.nlink = xshort(1);
    din.size = xint(0);
    winode(inum, &din);
    return inum;
}

void
balloc(int used)
{
    uchar buf[BSIZE];
    int i;

    //printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < BSIZE*8);
    bzero(buf, BSIZE);
    for(i = 0; i < used; i++){
        buf[i/8] = buf[i/8] | (0x1 << (i%8));
    }
    //printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
    wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n)
{
    char *p = (char*)xp;
    uint fbn, off, n1;
    struct dinode din;
    char buf[BSIZE];
    uint indirect[NINDIRECT];
    uint x;

    rinode(inum, &din);
    off = xint(din.size);
    //printf("append inum %d at off %d sz %d, fbn=%d\n", inum, off, n, off/BSIZE);
    while(n > 0){
        fbn = off / BSIZE;
        assert(fbn < MAXFILE);
        if (fbn < NDIRECT) {
            if (xint(din.addrs[fbn]) == 0) {
                din.addrs[fbn] = xint(freeblock++);
            }
            x = xint(din.addrs[fbn]);
        } else {
            if (xint(din.addrs[NDIRECT]) == 0) {
                din.addrs[NDIRECT] = xint(freeblock++);
            }
            rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            if (indirect[fbn - NDIRECT] == 0) {
                indirect[fbn - NDIRECT] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            }
            x = xint(indirect[fbn-NDIRECT]);
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
