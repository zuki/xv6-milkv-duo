#ifndef INC_BUF_H
#define INC_BUF_H

#include <common/types.h>
#include <list.h>

#define DSIZE 1024
#define B_BUSY  0x1     /* Buffer is lockd by some process */
#define B_VALID 0x2     /* Buffer has been read from disk. */
#define B_DIRTY 0x4     /* Buffer needs to be written to disk. */

struct buf {
    uint32_t flags;     // データをディスクから読み噛んでいるか?
    //int disk;         // ディスクはバッファを所有しているか?
    uint32_t dev;
    uint32_t blockno;
    uint32_t refcnt;
    struct list_head clink;     /* LRU cache list. */
    struct list_head dlink;     /* Disk buffer list. */
    uint8_t data[DSIZE];
};

#endif
