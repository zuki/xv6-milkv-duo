#ifndef INC_COMMON_FS_H
#define INC_COMMON_FS_H

#include <common/types.h>
#include <sd.h>
#include <linux/time.h>

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
#define MAXFILE     (NDIRECT + NINDIRECT)   // 1ファイルの最大ブロック数

// ディスク上のinode構造体(128バイト)
struct dinode {
    short type;                 // ファイルタイプ
    short major;                // メジャーデバイス番号 (T_DEVICE only)
    short minor;                // マイナーデバイス番号 (T_DEVICE only)
    short nlink;                // inodeへのリンク数
    uint32_t size;              // ファイルサイズ（バイト単位）(bytes)
    mode_t mode;                // ファイルモード
    uid_t  uid;                 // 所有者のユーザーID
    gid_t  gid;                 // 所有者のグループID
    struct timespec atime;      // 最新アクセス日時
    struct timespec mtime;      // 最新更新日時
    struct timespec ctime;      // 作成日時
    uint32_t addrs[NDIRECT+1];  // データブロックのアドレス
    char _dummy[4];
};

// ブロックあたりのInode数 = 1024 / 128 = 8
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

struct dirent64 {
  ino_t d_ino;
  off_t d_off;
  unsigned short d_reclen;
  unsigned char d_type;
  char d_name[256];
};

#endif
