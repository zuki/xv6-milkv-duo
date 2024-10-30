#ifndef INC_BUF_H
#define INC_BUF_H

#include <common/types.h>
#include "sleeplock.h"

#define DSIZE 1024

struct buf {
    int valid;        // データをディスクから読み噛んでいるか?
    int disk;         // ディスクはバッファを所有しているか?
    uint32_t dev;
    uint32_t blockno;
    struct sleeplock lock;
    uint32_t refcnt;
    struct buf *prev; // LRU キャッシュリスト
    struct buf *next;
    uchar data[DSIZE];
};

#endif
