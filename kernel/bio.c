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
    uint32_t blockno = fs_lba(dev) + bno * 2;
    trace("bno: %d, blockno: 0x%08x", bno, blockno);

    acquire(&bcache.lock);

    // 指定のブロックがキャッシュにあるか?
    // get_block()にあたる.ただし、load_block()はしない
loop:
    list_foreach(b, &bcache.head, clink) {
        if (b->dev == dev && b->blockno == blockno) {
            trace("bno: %d is cached", blockno);
            if (!(b->flags & B_BUSY)) {
                b->flags |= B_BUSY;
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
            trace("bno: %d is recycled", blockno);
            release(&bcache.lock);
            return b;
        }
    }
    //debug("bno: %d", bno);
    for (int i = 0; i < NBUF; i++) {
        int f = bcache.buf[i].flags;
        debug("buf[%d]: bn: %d, bvd: %d%d%d", i, bcache.buf[i].blockno, (f % B_BUSY) ? 1 : 0, (f % B_VALID) ? 1 : 0, (f % B_DIRTY) ? 1 : 0 )
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

// ロックされているバッファを解放する.
// 最も最近使用されたリストの先頭に移動する.
// TODO: release_block()
void brelse(struct buf *b)
{
    if ((b->flags & B_BUSY) == 0)
        panic("brelse");

    acquire(&bcache.lock);
    list_drop(&b->clink);
    list_push_back(&bcache.head, &b->clink);
    b->flags &= ~B_BUSY;
    trace("bno: %d, busy: %d, valid: %d, dirty: %d", b->blockno, (b->flags & B_BUSY) ? 1 : 0, (b->flags & B_VALID) ? 1 : 0, (b->flags & B_DIRTY) ? 1 : 0);
    wakeup(b);
    release(&bcache.lock);
}

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

// TODO: mark_block_dirty
