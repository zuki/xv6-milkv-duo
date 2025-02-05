// バッファキャッシュ.
//
// バッファキャッシュはディスクブロックの内容をキャッシュした
// コピーを保持する buf 構造体の連結リストである。 ディスク
// ブロックをメモリにキャッシュすることでディスクの読み込み
// 回数を減らすことができ、また、複数のプロセスで使用される
// ディスクブロックの同期ポイントにもなる。
//
// インタフェース:
// * ディスクの特定のブロックのバッファをシュトするには bread を呼び出す
// * バッファデータを変更したら bwrite を夜バイダしてディスクに書き戻す
// * バッファを使い終わったら、brelse を呼び出す
// * brelse 呼び出したらそのバッファを使用しない
// * 一度に1つのプロセスしかバッファは使用できない。そのため、必要以上に
//   長くバッファを保持しない
#include <common/types.h>
#include <common/param.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <common/riscv.h>
#include <defs.h>
#include <common/fs.h>
#include <buf.h>
#include <printf.h>
#include <memalign.h>
#include <common/fs.h>

static struct slab_cache *BUFDATA;

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // prev/nextによるすべてのバッファの連結リスト.
    // 使用されたもの順にソートされており、head.nextが
    // 最新で、head.prevが最も使用されていない
    struct list_head head;
} bcache;

// TODO: init_bufcache()
void binit(void)
{
    struct buf *b;
    BUFDATA = slab_cache_create("buf.data", 4096, ARCH_DMA_MINALIGN);

    initlock(&bcache.lock, "bcache");

    // バッファの連結リストを作成する
    list_init(&bcache.head);
    for(b = bcache.buf; b < bcache.buf+NBUF; b++){
        list_push_back(&bcache.head, &b->clink);
  }
}

// バッファキャッシュからデバイスdev のブロック番号 blockno の
// バッファを探す.
// 見つからなかったらバッファを割り当てる。
// どちらの場合もロックしたバッファを返す。
static struct buf *bget(uint32_t dev, uint32_t bno)
{
    struct buf *b;
    uint32_t blockno = fs_lba(dev) + bno * BLKSECT;
    trace("bno: %d, blockno: 0x%08x", bno, blockno);

    acquire(&bcache.lock);

    // 指定のブロックバッファがCLINKにあるか?
    // get_block()にあたる.ただし、load_block()はしない
loop:
    list_foreach(b, &bcache.head, clink) {
        if (b->dev == dev && b->blockno == blockno) {
            trace("cached: bno: 0x%x, ref: %d, flags: %d, data: %p", b->blockno, b->refcnt, b->flags, b->data);
            if (!(b->flags & B_BUSY)) {
                b->flags |= B_BUSY;
                b->refcnt++;
                release(&bcache.lock);
                return b;
            }
            sleep(b, &bcache.lock);
            goto loop;
        }
    }

    // キャッシュにない.
    // 未使用の最も利用されていないバッファをリサイクルする
    // find_free_entry()にあたる
    list_foreach_reverse(b, &bcache.head, clink) {
        if ((b->flags & B_BUSY) == 0 /* && b->refcnt == 0 */ && (b->flags & B_DIRTY) == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->flags = B_BUSY;
            b->refcnt = 1;
            if (b->data == NULL)
                b->data = (uint8_t *)slab_cache_alloc(BUFDATA);
            trace("recycle: buf: %p, &flags: %p, &dlink: %p, &data: %p", b, &b->flags, &b->dlink, &b->data);
            release(&bcache.lock);
            return b;
        }
    }
    //debug("bno: %d", bno);
    for (int i = 0; i < NBUF; i++) {
        int f = bcache.buf[i].flags;
        error("buf[%d]: bn: %d, bvd: %d%d%d", i, bcache.buf[i].blockno, (f % B_BUSY) ? 1 : 0, (f % B_VALID) ? 1 : 0, (f % B_DIRTY) ? 1 : 0 )
    }
    panic("bget: no buffers");
}

// 指定したブロック(ファイルシステムの先頭からの相対番号）のデータを
// 持つロックしたバッファを返す.
struct buf* bread(uint32_t dev, uint32_t bno)
{
    struct buf *b = bget(dev, bno);
    if ((b->flags & B_VALID) == 0) {
        sd_rw(b);
    }
    return b;
}

// バッファをディスクに書き戻す. ロックされていなければならない.
// TODO: write_entry()にあたる。dirtyフラグ処理あり
//       sync_bufcache()から呼び出す
// v6ではlogシステムのbegen_op()/end_op()のタイミングでのみ呼び出される
void bwrite(struct buf *b)
{
    if ((b->flags & B_BUSY) == 0) {
        error("bwrite: not locked dev: %d, blockno: 0x%x, flags=%d\n",
            b->dev, b->blockno, b->flags);
        panic("bwrite");
    }
    b->flags |= B_DIRTY;
    sd_rw(b);
}

// リリースするバッファのB_BUSYフラグを外しCLISTの末尾に移動する.
// TODO: release_block()
void brelse(struct buf *b)
{
    if ((b->flags & B_BUSY) == 0) {
        error("flags: 0x%x, dev: %d, bno: %d", b->flags, b->dev, b->blockno);
        panic("brelse");
    }

    acquire(&bcache.lock);
    list_drop(&b->clink);
    list_push_back(&bcache.head, &b->clink);
    b->refcnt--;
    b->flags &= ~B_BUSY;
    wakeup(b);
    release(&bcache.lock);
}

#if 0
void
bpin(struct buf *b) {
    acquire(&bcache.lock);
    b->refcnt++;
    release(&bcache.lock);
}

void
bunpin(struct buf *b) {
    acquire(&bcache.lock);
    b->refcnt--;
    release(&bcache.lock);
}
#endif
// TODO: mark_block_dirty
