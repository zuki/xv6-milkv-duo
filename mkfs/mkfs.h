#ifndef MKFS_H
#define MKFS_H

#define FSSIZE      102400

#define NINODES     1024
#define LOGSIZE     126
#define BSIZE       1024
#define NDIRECT     11
#define NINDIRECT   (BSIZE / sizeof(uint))
#define NINDIRECT2  (NINDIRECT * NINDIRECT)
#define MAXFILE     (NDIRECT + NINDIRECT + NINDIRECT2)
#define DIRSIZ      14
#define FSMAGIC     0x10203040
#define ROOTINO     1
#define IPB         (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb)   ((i) / IPB + sb.inodestart)

#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

#define min(a, b) ((a) < (b) ? (a) : (b))

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
    short type;                 // ファイルタイプ
    short major;                // メジャーデバイス番号 (T_DEVICE only)
    short minor;                // マイナーデバイス番号 (T_DEVICE only)
    short nlink;                // inodeへのリンク数
    uint  size;                 // ファイルサイズ（バイト単位）(bytes)
    mode_t mode;                // ファイルモード
    uid_t  uid;                 // 所有者のユーザーID
    gid_t  gid;                 // 所有者のグループID
    struct timespec atime;      // 最新アクセス日時
    struct timespec mtime;      // 最新更新日時
    struct timespec ctime;      // 作成日時
    uint  addrs[NDIRECT+2];     // データブロックのアドレス
    char _dummy[4];
};

struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

typedef unsigned char   uchar;

#endif
