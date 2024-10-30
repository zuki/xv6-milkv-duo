// バッファキャッシュ.
//
// バッファキャッシュはディスクブロックの内容をキャッシュした
// コピーを保持する buf 構造体の連結リストである。 ディスク
// ブロックをメモリにキャッシュすることでディスクの読み込み
// 回数を減らすことができ、また、複数のプロセスで使用される
// ディスクブロックの同期ポイントにもなる。
//
// インタフェース:
// * ぢスクの特定のブロックのバッファをシュトするには bread を呼び出す
// * バッファデータを変更したら bwrite を夜バイダしてディスクに書き戻す
// * バッファを使い終わったら、brelse を呼び出す
// * brelse 呼び出したらそのバッファを使用しない
// * 一度に1つのプロセスしかバッファは使用できない。そのため、必要以上に
//   長くバッファを保持しない


#include <common/types.h>
#include <common/param.h>
#include "spinlock.h"
#include "sleeplock.h"
#include <common/riscv.h>
#include "defs.h"
#include <common/fs.h>
#include "buf.h"

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // prev/nextによるすべてのバッファの連結リスト.
    // 使用されたもの順にソートされており、head.nextが
    // 最新で、head.prevが最も使用されていない
    struct buf head;
} bcache;

// TODO: init_bufcache()
void binit(void)
{
    struct buf *b;

    initlock(&bcache.lock, "bcache");

    // バッファの連結リストを作成する
    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;
    for(b = bcache.buf; b < bcache.buf+NBUF; b++){
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        initsleeplock(&b->lock, "buffer");
        bcache.head.next->prev = b;
        bcache.head.next = b;
  }
}

// バッファキャッシュからデバイスdev のブロック番号 blockno の
// バッファを探す.
// 見つからなかったらバッファを割り当てる。
// どちらの場合もロックしたバッファを返す。
static struct buf *bget(uint32_t dev, uint32_t blockno)
{
    struct buf *b;

    acquire(&bcache.lock);

    // 指定のブロックがキャッシュにあるか?
    // get_block()にあたる.ただし、load_block()はしない
    for (b = bcache.head.next; b != &bcache.head; b = b->next){
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // キャッシュにない.
    // 未使用の最も利用されていないバッファをリサイクルする
    // find_free_entry()にあたる
    for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
        if (b->refcnt == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }
    panic("bget: no buffers");
}

// 指定したブロックのデータを持つロックしたバッファを返す.
// TODO: read_entry()にあたる、load_block()から呼び出されている
//       vfsではget_block()
struct buf* bread(uint32_t dev, uint32_t blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        //virtio_disk_rw(b, 0);
        ramdiskrw(b, 0);          // TODO: vfs_bread
        b->valid = 1;
    }
    return b;
}

// バッファをディスクに書き戻す. ロックされていなければならない.
// TODO: write_entry()にあたる。dirtyフラグ処理あり
//       sync_bufcache()から呼び出す
// v6ではlogシステムのbegen_op()/end_op()のタイミングでのみ呼び出される
void bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    //virtio_disk_rw(b, 1);
    ramdiskrw(b, 1);              // TODO: vfs_bwrite
}

// ロックされているバッファを解放する.
// 最も最近使用されたリストの先頭に移動する.
// TODO: release_block()
void brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    acquire(&bcache.lock);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it.
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }

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
