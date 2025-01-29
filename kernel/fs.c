// ファイルシステムの実装.  5つのレイヤがある:
//   + ブロック: 生のディスクブロックのアロケータ.
//   + ログ: 多段階更新処理のためのクラッシュリカバリ機能.
//   + ファイル: inodeアロケータ、読み書きとメタデータ
//   + ディレクトリ: 特別な内容（他のinodeのリスト）を持つinode
//   + 名前: 便利な命名のための /usr/rtm/xv6/fs.c のようなパス.
//
// このファイルは低レベルのファイルシステム操作関数を含んでいる。
// （高レベルな）システムコールの実装はsysfile.c にある。

#include <common/types.h>
#include <common/riscv.h>
#include <defs.h>
#include <common/param.h>
#include <linux/stat.h>
#include <spinlock.h>
#include <proc.h>
#include <sleeplock.h>
#include <common/fs.h>
#include <buf.h>
#include <common/file.h>
#include <sd.h>
#include <printf.h>
#include <errno.h>
#include <linux/time.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

// TODO: vfs
// ディスク装置1台につき1つスーパーブロックがあるはずだが
// 我々は1台の装置しか使っていない。
struct superblock sb;

// スーパーブロックを読み込む
static void readsb(int dev, struct superblock *sb)
{
    struct buf *bp;

    bp = bread(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    brelse(bp);
}

static void printsb(struct superblock *sb) {
    debug("magic: 0x%08x, size: %d", sb->magic, sb->size);
    debug("num of block: %d, inode: %d, log: %d", sb->nblocks, sb->ninodes, sb->nlog);
    debug("start at log: %d, inode: %d, bmap: %d", sb->logstart, sb->inodestart, sb->bmapstart);
}

// ファイルシステムを初期化する
void fsinit(int dev) {
    readsb(dev, &sb);
    //printsb(&sb);
    if (sb.magic != FSMAGIC) {
        panic("invalid file system");
    }
    initlog(dev, &sb);
    info("fsinit ok");
}

// ブロックをゼロで初期化する.
static void bzero(int dev, int bno)
{
    struct buf *bp;

    bp = bread(dev, bno);
    memset(bp->data, 0, BSIZE);
    // FIXME: logシステムを削除する
    //log_write(bp);
    printf("a");
    bwrite(bp);
    brelse(bp);
}

// ブロックレイヤの関数.

// ゼロクリアしたディスクブロックを割り当て、そのブロック番号を返す。
// ディスクに空きがない場合は 0 を返す.
static uint32_t balloc(uint32_t dev)
{
    int b, bi, m;
    struct buf *bp;

    bp = 0;
    for (b = 0; b < sb.size; b += BPB) {
        bp = bread(dev, BBLOCK(b, sb));
        for (bi = 0; bi < BPB && b + bi < sb.size; bi++){
            m = 1 << (bi % 8);
            if ((bp->data[bi/8] & m) == 0) {  // Is block free?
                bp->data[bi/8] |= m;  // Mark block in use.
                //log_write(bp);
                printf("b");
                bwrite(bp);
                brelse(bp);
                bzero(dev, b + bi);
                return b + bi;
            }
        }
        brelse(bp);
    }
    printf("balloc: out of blocks\n");
    return 0;
}

// ディスクブロックを解放する.
static void bfree(int dev, uint32_t b)
{
    struct buf *bp;
    int bi, m;

    bp = bread(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if ((bp->data[bi/8] & m) == 0)
        panic("freeing free block");
    bp->data[bi/8] &= ~m;
    //log_write(bp);
    printf("c");
    bwrite(bp);
    brelse(bp);
}

// inodeレイヤの関数.
//
// inodeは名前のない1つのファイルを記述する。
// dinode構造体はファイルのタイプ、サイズ、そのファイルを参照する
// リンク数、ファイルコンテンツを保持するブロックのリストなどの
// メタデータを保持する。
//
// inodeはディスク上のsb.inodestartブロックから順番に並べられる。
// 各inodeはディスク上の位置を示す番号を持っている。
//
// カーネルは複数のプロセスで使用されるinodeへのアクセスを同期する
// 場所を提供するために使用中のinodeのテーブルをメモリ内に保持する。
// インメモリinodeにはディスクに保存されないブックキーピング情報
// （ip->refとip->valid）が含まれる。
//
// inodeとそのメモリ内表現はファイルシステムコードの残りの部分で
// 使用できるようになる前に一連の状態を通過する。
//
// * 割り当て: inodeはタイプ(dinode.type)が非ゼロの場合、
// 割り当てられる。ialloc()は割り当てを行い、iput()は参照
// カウントとリンクカウントがゼロになった場合に解放を行う。
//
// * テーブルの参照: inodeテーブルのエントリはip->refがゼロに
// なると解放される。そうでなければ、ip->refはエントリ（オープン
// ファイルや関連とディレクトリ）へのインメモリポインタの数を
// 追跡する。iget()はテーブルエントリを見つけるか作成して、
// そのrefをインクリメントする。iput()はrefをデクリメントする。
//
// * 有効性: inodeテーブルエントリの情報(タイプ、サイズなど)は
// ip->validが1のときのみ正しい。ilock()はinodeをディスクから
// 夜m込み、ip->validをセットする。一方、iput()はip->refが0に
// なった場合、ip->validをクリアする。
//
// * ロック: ファイルシステムのコードはまずinodeをロックしないと
// inodeの情報とその内容を調べたり変更したりすることができない。
//
// したがって、通常、処理シーケンスは次のようになる。
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock()とiget()は分離されているのでシステムコールは(オープン
// ファイルなどの）inodeへの長期的な参照を取得することができ
// (read()などで)短期間だけロックすることができる。この分離は
// パス名検索の際のデッドロックや教護を避けるのにも役立っている。
// iget()はip->refをインクリメントするのでinodeはテーブル内に
// 留まり、それへのポインタの有効性は保持される。
//
// 多くの内部ファイルシステム関数は呼び出し元が関係するinodeを
// ロックしていることを想定している。これにより呼び出し元は
// 多段階のアトミック操作が可能である。
//
// itable.lockスピンロックはitableエントリの割り当てを保護する。
// ip->refはエントリがフリーかどうかを示し、ip->devとip->inumは
// エントリが保持するi-nodeを示すので、これらのフィールドのいずれかを
// 使用している間、itable.lockを保持しなければならない。
//
// ip->lockスリープロックはref、dev、inum以外のすべてのip->
// フィールドを保護する。inodeのip->valid、ip->size、ip->typeなどを
// 読み書きするにはip->lockを保持しなければならない。

// インメモリinodeテーブル構造体
struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

// inodeテーブルを初期化する
void iinit()
{
    int i = 0;

    initlock(&itable.lock, "itable");
    for (i = 0; i < NINODE; i++) {
        initsleeplock(&itable.inode[i].lock, "inode");
    }
}

static struct inode* iget(uint32_t dev, uint32_t inum);

// デバイスdevにinodeを割り当てる.
// タイプtypeを与えることによりinodeを割り当て済みとマークする.
// ロックしていない割り当て済みで参照済みのinodeを返す.
// 空きinodeがなかった場合はNULLを返す
struct inode* ialloc(uint32_t dev, short type)
{
    int inum;
    struct buf *bp;
    struct dinode *dip;

    for (inum = 1; inum < sb.ninodes; inum++) {
        bp = bread(dev, IBLOCK(inum, sb));
        dip = (struct dinode*)bp->data + inum%IPB;
        if (dip->type == 0) {  // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            //log_write(bp);   // mark it allocated on the disk
            printf("d");
            bwrite(bp);
            brelse(bp);
            struct inode *ip = iget(dev, inum);
            struct timespec tp;
            rtc_gettime(&tp);
            ip->atime = ip->ctime = ip->mtime = tp;
            return ip;
        }
        brelse(bp);
    }
    printf("ialloc: no inodes\n");
    return 0;
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
void
iupdate(struct inode *ip)
{
    struct buf *bp;
    struct dinode *dip;

    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    dip->uid   = ip->uid;
    dip->gid   = ip->gid;
    dip->mode  = ip->mode;
    dip->atime = ip->atime;
    dip->mtime = ip->mtime;
    dip->ctime = ip->ctime;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    //log_write(bp);
    printf("e");
    bwrite(bp);
    brelse(bp);
}

// デバイスdevで番号がinumのinodeをさがして、その
// インメモリコピーを返す。ロックはせず、ディスク
// から読み込むこともない.
static struct inode*
iget(uint32_t dev, uint32_t inum)
{
    struct inode *ip, *empty;

    acquire(&itable.lock);

    // Is the inode already in the table?
    empty = 0;
    for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            ip->ref++;
            release(&itable.lock);
            trace("find: dev: %d, inum: %d, ref: %d", dev, inum, ip->ref);
            return ip;
        }
        if(empty == 0 && ip->ref == 0)    // Remember empty slot.
            empty = ip;
    }

    // Recycle an inode entry.
    if (empty == 0)
        panic("iget: no inodes");

    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    release(&itable.lock);
    trace("recycle: dev: %d, inum: %d", dev, inum);
    return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
    acquire(&itable.lock);
    ip->ref++;
    release(&itable.lock);
    return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
    struct buf *bp;
    struct dinode *dip;

    if (ip == 0 || ip->ref < 1)
        panic("ilock");

    acquiresleep(&ip->lock);

    if (ip->valid == 0) {
        bp = bread(ip->dev, IBLOCK(ip->inum, sb));
        dip = (struct dinode*)bp->data + ip->inum%IPB;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        ip->mode  = dip->mode;
        ip->uid   = dip->uid;
        ip->gid   = dip->gid;
        ip->atime = dip->atime;
        ip->mtime = dip->mtime;
        ip->ctime = dip->ctime;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        brelse(bp);
        ip->valid = 1;
        if(ip->type == 0)
            panic("ilock: no type");
    }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
    if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");

    releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput(struct inode *ip)
{
    acquire(&itable.lock);

    if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
        // inode has no links and no other references: truncate and free.

        // ip->ref == 1 means no other process can have ip locked,
        // so this acquiresleep() won't block (or deadlock).
        acquiresleep(&ip->lock);

        release(&itable.lock);

        itrunc(ip);
        ip->type = 0;
        iupdate(ip);
        ip->valid = 0;

        releasesleep(&ip->lock);

        acquire(&itable.lock);
    }

    ip->ref--;
    release(&itable.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
    iunlock(ip);
    iput(ip);
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// returns 0 if out of disk space.
static uint32_t
bmap(struct inode *ip, uint32_t bn)
{
    uint32_t idx1, idx2, addr, *a;
    struct buf *bp;
    trace("ip: %d, bn: %d", ip->inum, bn);
    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[bn] = addr;
        }
        return addr;
    }

    bn -= NDIRECT;

    if (bn < NINDIRECT) {
        // Load indirect block, allocating if necessary.
        if ((addr = ip->addrs[NDIRECT]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[NDIRECT] = addr;
        }
        bp = bread(ip->dev, addr);
        a = (uint32_t*)bp->data;
        if ((addr = a[bn]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            a[bn] = addr;
            //log_write(bp);
            printf("f: flags=0x%x", bp->flags);
            bwrite(bp);
        }
        trace("bn: %d, addr: 0x%lx", bn, addr);
        brelse(bp);
        return addr;
    }

    bn -= NINDIRECT;

    if (bn < NINDIRECT2) {
        // Load indirect block, allocating if necessary.
        if ((addr = ip->addrs[NDIRECT+1]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[NDIRECT+1] = addr;
        }
        idx1 = bn / NINDIRECT;
        idx2 = bn % NINDIRECT;
        bp = bread(ip->dev, addr);
        a = (uint32_t*)bp->data;
        if ((addr = a[idx1]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            a[idx1] = addr;
            //log_write(bp);
            printf("g: flags=0x%x", bp->flags);
            bwrite(bp);
        }
        trace("idx1: %d, addr1: 0x%lx", idx1, addr);
        brelse(bp);
        bp = bread(ip->dev, addr);
        a = (uint32_t*)bp->data;
        if ((addr = a[idx2]) == 0) {
            a[idx2] = addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            a[idx2] = addr;
            //log_write(bp);
            printf("h: flags=0x%x", bp->flags);
            bwrite(bp);
        }
        trace("idx2: %d, addr2: 0x%lx", idx2, addr);
        brelse(bp);
        return addr;
    }
    panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void
itrunc(struct inode *ip)
{
    int i, j;
    uint32_t *a, *b;
    struct buf *bp;

    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT]);
        a = (uint32_t*)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            if (a[j]) {
                bfree(ip->dev, a[j]);
                a[j] = 0;
            }
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    if (ip->addrs[NDIRECT+1]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
        a = (uint32_t *)bp->data;
        for (i = 0; i < NINDIRECT; i++) {
            if (a[i]) {
                brelse(bp);
                bp = bread(ip->dev, a[i]);
                b = (uint32_t *)bp->data;
                for (j = 0; j < NINDIRECT; j++) {
                    if (b[j]) {
                        bfree(ip->dev, b[j]);
                        b[j] = 0;
                    }
                }
                bfree(ip->dev, a[i]);
                a[i] = 0;
            }
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT+1]);
        ip->addrs[NDIRECT+1] = 0;
    }

    ip->size = 0;
    iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.

void stati(struct inode *ip, struct stat *st)
{
    // FIXME: Support other fields in stat.
    st->st_dev     = ip->dev;
    st->st_ino     = ip->inum;
    st->st_nlink   = ip->nlink;
    st->st_uid     = ip->uid;
    st->st_gid     = ip->gid;
    st->st_size    = ip->size;
    st->st_blksize = 1024;
    st->st_blocks  = (ip->size / st->st_blksize) + 1;
    st->st_atime   = ip->atime;
    st->st_mtime   = ip->mtime;
    st->st_ctime   = ip->ctime;
    if (ip->type == T_DEVICE)
        st->st_rdev = mkdev(ip->major, ip->minor);

    switch (ip->type) {
    case T_FILE:
    case T_DIR:
    case T_DEVICE:
    case T_MOUNT:
    case T_SYMLINK:
        st->st_mode = ip->mode;
        break;
    default:
        panic("unexpected stat type");
    }
}

// inodeからデータを読み込む.
// Callerはip->lockを保持していなければならない.
// user_dst==1 の場合、dst はユーザ仮想アドレス、
// そうでなければ、dst はカーネルアドレス.
int
readi(struct inode *ip, int user_dst, uint64_t dst, uint32_t off, uint32_t n)
{
    uint32_t tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off)
        return 0;
    if (off + n > ip->size)
        n = ip->size - off;

    for (tot=0; tot<n; tot+=m, off+=m, dst+=m) {
        uint32_t addr = bmap(ip, off/BSIZE);
        if (addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m = min(n - tot, BSIZE - off%BSIZE);
        if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
            brelse(bp);
            tot = -1;
            break;
        }
        brelse(bp);
    }
    return tot;
}

// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
int
writei(struct inode *ip, int user_src, uint64_t src, uint32_t off, uint32_t n)
{
    uint32_t tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE*BSIZE)
        return -1;

    for (tot=0; tot<n; tot+=m, off+=m, src+=m) {
        uint32_t addr = bmap(ip, off/BSIZE);
        if (addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m = min(n - tot, BSIZE - off%BSIZE);
        if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
            brelse(bp);
            break;
        }
        //log_write(bp);
        printf("i");
        bwrite(bp);
        brelse(bp);
    }

    if (off > ip->size)
        ip->size = off;

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    iupdate(ip);

    return tot;
}

// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint32_t *poff)
{
    uint32_t off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        panic("dirlookup not DIR");

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        if(de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0) {
            // entry matches path element
            if (poff)
                *poff = off;
            inum = de.inum;
            return iget(dp->dev, inum);
        }
    }

    return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
// Returns 0 on success, -1 on failure (e.g. out of disk blocks).
int
dirlink(struct inode *dp, char *name, uint32_t inum)
{
    int off;
    struct dirent de;
    struct inode *ip;

    // Check that name is not present.
    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iput(ip);
        return -1;
    }

    // Look for an empty dirent.
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        if (de.inum == 0)
            break;
    }

    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
        return -1;

    return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
    char *s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
    struct inode *ip, *next;

    if (*path == '/')
        ip = iget(ROOTDEV, ROOTINO);
    else
        ip = idup(myproc()->cwd);

    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR){
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // Stop one level early.
            iunlock(ip);
            return ip;
        }
        if ((next = dirlookup(ip, name, 0)) == 0) {
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if (nameiparent) {
        iput(ip);
        return 0;
    }
    return ip;
}

struct inode*
namei(char *path)
{
    char name[DIRSIZ];
    return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
    return namex(path, 1, name);
}

int unlink(struct inode *dp, uint32_t off)
{
    struct dirent de;
    // FIXME: 取り詰めとsizeの変更
    memset(&de, 0, sizeof(de));
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
        return -1;
    return 0;
}

int getdents(struct file *f, uint64_t data, size_t size)
{
    ssize_t n;
    int namelen, reclen, tlen = 0, off = 0;
    struct dirent de;
    struct dirent64 de64;
    struct inode *dirip;

    trace("ip: %d, data: 0x%lx, size: %ld", f->ip->inum, data, size);
    while (1) {
        n = fileread(f, (uint64_t)&de, sizeof(struct dirent), 0);
        //debug("n: %kd, de: de.inum: %d, name: %s", n, de.inum, de.name);

        //r = (buf - data);
        if (n == 0) {
            //return tlen;
            //error("read 0, tlen=%ld", tlen);
            //return tlen ? tlen : -ENOENT;
            break;
        }
        if (n < 0 || n != sizeof(struct dirent)) {
            error("readi invalid n=%ld, tlen=%ld", n, tlen);
            return tlen ? tlen : -1;
        }

        if (de.inum == 0)
            continue;

        namelen = MIN(strlen(de.name), DIRSIZ) + 1;
        reclen = (size_t)(&((struct dirent64*)0)->d_name);
        reclen = reclen + namelen;
        reclen = (reclen + 7) & ~0x7;
        if ((tlen + reclen) > size) {
            error("break; tlen: %d, reclen: %d, size: %d", tlen, reclen, size);
            break;
        }
        trace("inum: %d, type: %d, namelen: %d, reclen: %d", de.inum, f->ip->type, namelen, reclen);

        //de64 = (struct dirent64 *)buf;
        //memset(de64, 0, sizeof(struct dirent64));
        de64.d_ino = de.inum;
        de64.d_off = off;
        de64.d_reclen = reclen;

        dirip = iget(f->ip->dev, de.inum);
        ilock(dirip);
        trace("dirip inum: %d, type: %d", dirip->inum, dirip->type);
        //de64.d_type = IFTODT(f->ip->mode);
        if (dirip->type == T_DEVICE) {
            if (f->major == 0)      // SD
                de64.d_type = IFTODT(S_IFBLK);
            else // if (f->major == 1) // CONSOLE
                de64.d_type = IFTODT(S_IFCHR);
        } else if (dirip->type == T_DIR) {
            de64.d_type = IFTODT(S_IFDIR);
        } else { // if (dirip->type == T_FILE)
            de64.d_type = IFTODT(S_IFREG);
        }
        iunlockput(dirip);

        strncpy(de64.d_name, de.name, namelen);

        if (copyout(myproc()->pagetable, data + tlen, (char *)&de64, reclen) < 0) {
            error("failed copyout");
            return -EFAULT;
        }

        tlen += reclen;
        off = f->off;
        trace("tlen: %d, de64: ino: %d, off: %ld, reclen: %d, type: %d, name: %s", tlen, de64.d_ino, de64.d_off, de64.d_reclen, de64.d_type, de64.d_name);
    }
    //debug_bytes(data, tlen);
    return tlen;
}
