#ifndef MKFS_H
#define MKFS_H

#define FSSIZE      1500

#define NINODES     256
#define LOGSIZE     30
#define BSIZE       1024
#define NDIRECT     12
#define NINDIRECT   (BSIZE / sizeof(uint))
#define MAXFILE     (NDIRECT + NINDIRECT)
#define DIRSIZ      14
#define FSMAGIC     0x10203040
#define ROOTINO     1
#define IPB         (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb)   ((i) / IPB + sb.inodestart)

#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct superblock {
    uint magic;        // Must be FSMAGIC
    uint size;         // ファイルシステムのサイズ（ブロック単位）
    uint nblocks;      // データブロックの総数
    uint ninodes;      // inodeの総数.
    uint nlog;         // ログブロックの総数
    uint logstart;     // 先頭のログブロックのブロック番号
    uint inodestart;   // 先頭のinodeブロックのブロック番号
    uint bmapstart;    // 先頭のビットマップブロックのブロック番号
};

struct dinode {
    short type;         // ファイルタイプ
    short major;        // メジャーデバイス番号 (T_DEVICE only)
    short minor;        // マイナーデバイス番号 (T_DEVICE only)
    short nlink;        // inodeへのリンク数
    uint  size;      // ファイルサイズ（バイト単位）(bytes)
    uint  addrs[NDIRECT+1];   // データブロックのアドレス
};

struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

typedef unsigned char   uchar;

#endif