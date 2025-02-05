#include <sd.h>
#include <mmc.h>
#include <defs.h>
#include <list.h>
#include <common/riscv.h>
#include <spinlock.h>
#include <buf.h>
#include <common/types.h>
#include <printf.h>
#include <riscv-barrier.h>

static struct mmc sd0;
static struct list_head sdque;
static struct spinlock sdlock;
struct partition_info ptinfo[PARTITIONS];

// 使用中のパーティション数
static int ptnum = 0;

static void sd_sleep(void *chan)
{
    sleep(chan, &sdlock);
}

/*
 * Initialize SD card and parse MBR.
 * 1. The first partition should be FAT and is used for booting.
 * 2. The second partition is used by our file system.
 *
 * See https://en.wikipedia.org/wiki/Master_boot_record
 */
void
sd_init(void)
{
    struct mbr mbr;

    char buf[1024];

    list_init(&sdque);
    initlock(&sdlock, "sd");

    acquire(&sdlock);
    int ret = mmc_initialize(&sd0);
    //assert(ret == 0);
    if (ret)
      panic("failed mmc_initialize\n");

    size_t blks = mmc_bread_mbr(&sd0, buf);
    if (blks != 2) {
        error("read blks: %ld", blks);
        panic("failed mmc_bread\n");
    }

    release(&sdlock);

    //assert(mbr.signature == 0xAA55);

    memmove(&mbr, buf, 512);
    for (int i = 0; i < PARTITIONS; i++) {
        if (mbr.ptables[i].lba == 0) break;

        ptinfo[i].type = mbr.ptables[i].type;
        ptinfo[i].lba = mbr.ptables[i].lba;
        ptinfo[i].nsecs = mbr.ptables[i].nsecs;
        info("partition[%d]: TYPE: %d, LBA = 0x%x, #SECS = 0x%x",
            i, ptinfo[i].type, ptinfo[i].lba, ptinfo[i].nsecs);
        ptnum++;
    }

    info("sd_init ok\n");
}

void
sd_intr(void)
{
    acquire(&sdlock);
    wakeup(&sd0);
    release(&sdlock);
}


/*
 * SDカードのリクエスト処理を開始する.
 * Callerはsdlockを保持していなければならない.
 */
static void sd_start(void)
{
    while (!list_empty(&sdque)) {
        struct buf *b =
            container_of(list_front(&sdque), struct buf, dlink);

        // TODO: block sizeをvfsで持つ
        uint32_t blks = b->dev == FATMINOR ? 1 : BLKSECT;
        trace("buf blockno: 0x%08x, blks: %d, flags: 0x%08x", b->blockno, blks, b->flags);

        if (b->flags & B_DIRTY) {
            assert(mmc_bwrite(&sd0, b->blockno, blks, b->data) == blks);
        } else {
            assert(mmc_bread(&sd0, b->blockno, blks, b->data) == blks);
        }

        b->flags |= B_VALID;
        b->flags &= ~B_DIRTY;

        list_pop_front(&sdque);

        wakeup(b);
    }
}

void sd_rw(struct buf *b)
{
    acquire(&sdlock);
    trace("bno: 0x%x, flags: %d", b->blockno, b->flags);

    // Append to request queue.
    list_push_back(&sdque, &b->dlink);

    // Start disk if necessary.
    if (list_front(&sdque) == &b->dlink)
        sd_start();

    // Wait for request to finish.
    while ((b->flags & (B_VALID | B_DIRTY)) != B_VALID)
        sd_sleep(b);

    release(&sdlock);
}
