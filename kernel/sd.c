#include <sd.h>
#include <emmc.h>
#include <defs.h>
#include <list.h>
#include <common/riscv.h>
#include <spinlock.h>
#include <buf.h>
#include <common/types.h>
#include <printf.h>

static struct emmc sd0;
static struct list_head sdque;
static struct spinlock sdlock;
struct partition_info ptinfo[PARTITIONS];

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
    char buf[1024];

    list_init(&sdque);
    initlock(&sdlock, "sd");

    acquire(&sdlock);
    int ret = emmc_init(&sd0);
    //assert(ret == 0);
    if (ret)
      panic("failed emmc_init\n");
    emmc_seek(&sd0, 0UL);
    size_t bytes = emmc_read(&sd0, buf, 1024);
    if (bytes != 1024)
      panic("failed emmc_read\n");
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

    info("sd_init ok\n");
}

void
sd_intr(void)
{
    acquire(&sdlock);
    emmc_intr(&sd0);
    //disb();
    wakeup(&sd0);
    release(&sdlock);
}


/*
 * SDカードのリクエスト処理を開始する.
 * Callerはsdlockを保持していなければならない.
 */
void sd_start(void)
{
    //uint32_t bno;

    while (!list_empty(&sdque)) {
        struct buf *b =
            container_of(list_front(&sdque), struct buf, dlink);
        // TODO: block sizeをvfsで持つ
        uint32_t blks = b->dev == FATMINOR ? 512 : 1024;
        trace("buf blockno: 0x%08x, flags: 0x%08x", b->blockno, b->flags);
        emmc_seek(&sd0, b->blockno * SD_BLOCK_SIZE);

        if (b->flags & B_DIRTY) {
            assert(emmc_write(&sd0, b->data, blks) == blks);
        } else {
            assert(emmc_read(&sd0, b->data, blks) == blks);
        }

        b->flags |= B_VALID;
        b->flags &= ~B_DIRTY;

        list_pop_front(&sdque);

        //disb();
        wakeup(b);
    }
}


void sd_rw(struct buf *b)
{
    acquire(&sdlock);

    // Append to request queue.
    list_push_back(&sdque, &b->dlink);

    // Start disk if necessary.
    if (list_front(&sdque) == &b->dlink) {
        sd_start();
    }

    // Wait for request to finish.
    while ((b->flags & (B_VALID | B_DIRTY)) != B_VALID)
        sd_sleep(b);

    release(&sdlock);
}
