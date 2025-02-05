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

//static struct slab_cache *SDBUF;

// 使用中のパーティション数
static int ptnum = 0;

// Hack the partition.
//static uint32_t first_bno = 0;
//static uint32_t nblocks = 1;

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

    //SDBUF = slab_cache_create("sd buffer", 1024);

    //char *buf = slab_cache_alloc(SDBUF);
    char buf[1024];

    list_init(&sdque);
    initlock(&sdlock, "sd");

    acquire(&sdlock);
    //int ret = emmc_init(&sd0);
    int ret = mmc_initialize(&sd0);
    //assert(ret == 0);
    if (ret)
      panic("failed mmc_initialize\n");
#if 0
    emmc_seek(&sd0, 0UL);
    size_t bytes = emmc_read(&sd0, buf, 1024);
#endif
    size_t blks = mmc_bread_mbr(&sd0, buf);
    if (blks != 2) {
        error("read blks: %ld", blks);
        panic("failed mmc_bread\n");
    }

    //assert(bytes == 512);
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

#if 0
    acquire(&sdlock);
    emmc_seek(&sd0, 512UL * (ptinfo[1].lba + 1024));
    bytes = emmc_read(&sd0, buf, 512);
    if (bytes != 512)
      panic("failed emmc_read\n");
    //assert(bytes == 512);
    release(&sdlock);

    uint32_t byte = 0;
    printf("\n");
    for (int i=0; i < 32; i++) {
      for (int j=0; j < 16; j++) {
        if (j == 0)
          printf("%08x:", byte);
        if (j%2)
          printf("%02x", buf[i*16+j]);
        else
          printf(" %02x", buf[i*16+j]);
      }
      printf("\n");
      byte += 16;
    }
#endif

    //slab_cache_free(SDBUF, buf);

    info("sd_init ok\n");
}

void
sd_intr(void)
{
    acquire(&sdlock);
    //emmc_intr(&sd0);
    //disb();
    wakeup(&sd0);
    release(&sdlock);
}


/*
 * SDカードのリクエスト処理を開始する.
 * Callerはsdlockを保持していなければならない.
 */
static void sd_start(void)
{
    //char *buf = slab_cache_alloc(SDBUF);

    while (!list_empty(&sdque)) {
        struct buf *b =
            container_of(list_front(&sdque), struct buf, dlink);

        // TODO: block sizeをvfsで持つ
        uint32_t blks = b->dev == FATMINOR ? 1 : BLKSECT;
        trace("buf blockno: 0x%08x, blks: %d, flags: 0x%08x", b->blockno, blks, b->flags);
        trace("[0] buf: %p, &b->data: %p, b->data: %p", b, &b->data, b->data);

        if (b->flags & B_DIRTY) {
            //memmove(buf, b->data, blks * 512);
            //mb();
            //fence_i();
            assert(mmc_bwrite(&sd0, b->blockno, blks, b->data) == blks);
        } else {
            assert(mmc_bread(&sd0, b->blockno, blks, b->data) == blks);
            //mb();
            //fence_i();
            //memmove(b->data, buf, blks * 512);
        }

#if 0
        uint32_t byte = 0;
        printf("\n");
        for (int i=0; i < 32; i++) {
            for (int j=0; j < 16; j++) {
                if (j == 0)
                printf("%08x:", byte);
                if (j%2)
                printf("%02x", b->data[i*16+j]);
                else
                printf(" %02x", b->data[i*16+j]);
            }
            printf("\n");
            byte += 16;
        }
#endif
        trace("[1] buf: %p, &b->data: %p, b->data: %p", b, &b->data, b->data);

        b->flags |= B_VALID;
        b->flags &= ~B_DIRTY;

#if 0
        struct list_head *item = list_front(&sdque);
        if (item->next == 0) {
            warn("item->next is null");
            item->next = &sdque;
        } else {
            debug("item->next: 0x%x", item->next);
        }

        if (item->prev == 0) {
            warn("item->prev is null");
            item->prev= &sdque;
        } else {
            debug("item->prev: 0x%x", item->prev);
        }

        list_drop(item);
#endif

        list_pop_front(&sdque);
        //slab_cache_free(SDBUF, buf);

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
