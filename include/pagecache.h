#ifndef INC_PAGECACHE_H
#define INC_PAGECACHE_H

#include <types.h>
#include <file.h>
#include <sleeplock.h>
#include <spinlock.h>

#define NPAGECACHE 10240

struct cached_page {
    char *page;
    uint32_t dev;
    uint32_t inum;
    int ref_count;
    off_t offset;
    struct sleeplock lock;
};

#endif
