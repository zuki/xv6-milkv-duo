#ifndef INC_SD_H
#define INC_SD_H

#include "buf.h"
#include <common/types.h>

#define PARTITIONS      4       // 最大パーティション数
#define SECTOR_SIZE     512     // セクタサイズ（バイト）

// FIXME: vfsを実装したらvfs.hに移動
#define SDMAJOR         0                   // SD card major block device
#define FATMINOR        0                   // FAT partition [0,0]
#define XV6MINOR        1                   // xv6 partition [0.1]

struct ptable_entry {
  char      flag;         // ブートフラグ
  char      chs1[3];      // 最初のセクタ: CHS方式
  char      type;         // 種別
  char      chs2[3];      // 最後のセクタ: CHS方式
  uint32_t  lba;          // 最初のセクタ: LBA青色
  uint32_t  nsecs;        // 全セクタ数
} __attribute__((packed));
typedef struct ptable_entry ptentry_t;

// マスターブートレコード
struct mbr {
    char        _bootstrap[446];
    ptentry_t   ptables[4];
    uint16_t    signature;
} __attribute__((packed));


struct partition_info {
  uint32_t  lba;
  uint32_t  nsecs;
  char      type;
  uint8_t   name[23];
} __attribute__((packed));
typedef struct partition_info ptinfo_t;

/*
 * log_2(size): 512 = 9, 4096 = 12
 * assumes size > 256
 */
static inline uint8_t blksize_bits(uint32_t size)
{
  uint8_t bits = 8;
  do {
      bits++;
      size >>= 1;
    } while (size > 256);
  return bits;
}

extern struct partition_info ptinfo[PARTITIONS];

static inline uint32_t fs_lba(int  dev)
{
    return ptinfo[dev].lba;
}

#endif
