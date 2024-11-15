#ifndef INC_COMMON_FS_H
#define INC_COMMON_FS_H

#include <sd.h>

// ディスク上のファイルシステムフォーマット.
// カーネル/ユーザプログラムの両者がこのヘッダーファイルを使用する.

#define ROOTINO     1       // ルートディレクトリ('/')のinode番号
#define BSIZE       1024    // ブロックサイズ

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
    uint32_t magic;        // Must be FSMAGIC
    uint32_t size;         // ファイルシステムのサイズ（ブロック単位）
    uint32_t nblocks;      // データブロックの総数
    uint32_t ninodes;      // inodeの総数.
    uint32_t nlog;         // ログブロックの総数
    uint32_t logstart;     // 先頭のログブロックのブロック番号
    uint32_t inodestart;   // 先頭のinodeブロックのブロック番号
    uint32_t bmapstart;    // 先頭のビットマップブロックのブロック番号
};

#define FSMAGIC     0x10203040

#define NDIRECT     12      // 直接指定のブロック数
#define NINDIRECT   (BSIZE / sizeof(uint))  // 間接指定のブロック数
#define MAXFILE     (NDIRECT + NINDIRECT)   // 1ファイルの最大ブロックする

// ディスク上のinode構造体(64バイト)
struct dinode {
    short type;         // ファイルタイプ
    short major;        // メジャーデバイス番号 (T_DEVICE only)
    short minor;        // マイナーデバイス番号 (T_DEVICE only)
    short nlink;        // inodeへのリンク数
    uint32_t size;      // ファイルサイズ（バイト単位）(bytes)
    uint32_t addrs[NDIRECT+1];   // データブロックのアドレス
};

// ブロックあたりのInode数 = 1024 / 64 = 16
#define IPB             (BSIZE / sizeof(struct dinode))

// inode iを含むブロックの番号
#define IBLOCK(i, sb)   ((i) / IPB + sb.inodestart)
// ブロックあたりのビットマップビット数 = 8192
#define BPB             (BSIZE * 8)

// ブロック bを含むビットマップブロックの番号
#define BBLOCK(b, sb)   ((b) / BPB + sb.bmapstart)

// ディレクトリ名の最大長()
#define DIRSIZ          14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#endif
