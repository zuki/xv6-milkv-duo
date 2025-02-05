#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>

#define NDIRECT         11

struct ptable_entry {
  char      flag;         // ブートフラグ
  char      chs1[3];      // 最初のセクタ: CHS方式
  char      type;         // 種別
  char      chs2[3];      // 最後のセクタ: CHS方式
  uint32_t  lba;          // 最初のセクタ: LBA青色
  uint32_t  nsecs;        // 全セクタ数
} __attribute__((packed));

struct mbr {
    char        _bootstrap[446];
    struct ptable_entry   pentries[4];
    uint16_t    signature;
} __attribute__((packed));

struct superblock {
    uint32_t magic;        // Must be FSMAGIC
    uint32_t size;         // Size of file system image (blocks)
    uint32_t nblocks;      // Number of data blocks
    uint32_t ninodes;      // Number of inodes.
    uint32_t nlog;         // Number of log blocks
    uint32_t logstart;     // Block number of first log block
    uint32_t inodestart;   // Block number of first inode block
    uint32_t bmapstart;    // Block number of first free map block
};

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
    uint32_t addrs[NDIRECT+2];  // データブロックのアドレス
    char _dummy[4];
};

// FROM mksd.mk
// The total sd card image is 128 MB, 64 MB for boot sector and 64 MB for file system.
// 以下の単位はセクタ
#define SECTOR_SIZE     512
#define BUFSIZE         4096

#define NINODES         1024
#define FSSIZE          102400
#define BSIZE           4096
#define LOGSIZE         4
#define IPB             (BSIZE / sizeof(struct dinode))         // 1024 / 64 = 16

/* Disk layout: 1 block = BSIZE
    [ boot block | super-block | log-block | dinode block | bitmap block | data blocks ]
    以下の変数はセクタ単位(512B)
*/
const int nbitmap = FSSIZE / (BSIZE * 8) + 1;
const int ninode  = (NINODES / IPB) + ((NINODES % IPB) == 0 ? 0 : 1);
const int nlog    = LOGSIZE;
const int nmeta = 2 + nlog + ninode + nbitmap;
int data_start;

static char *TYPE_NAME[4] = { "-", "DIR", "FILE", "DEV" };
struct superblock sb;
struct mbr mbr;

void print_pentry(int i, struct ptable_entry *pe) {
    printf("pentry[%d]: type: %02x, LBA: 0x%08x, SIZE: 0x%08x\n", i, pe->type, pe->lba, pe->nsecs);
}

void get_partition_entry(FILE *f) {
    fseek(f, 0, SEEK_SET);
    if (fread(&mbr, sizeof(struct mbr), 1, f) != 1) {
        perror("mbr read");
        exit(1);
    }
#if 0
    for (int i = 0; i < 4; i++) {
        print_pentry(i, &mbr.pentries[i]);
    }
#endif
}

void get_superblock(FILE *f) {
    int n;

    get_partition_entry(f);

    fseek(f, mbr.pentries[1].lba * SECTOR_SIZE + BSIZE, SEEK_SET);
    n = fread(&sb, sizeof(struct superblock), 1, f);
    if (n != 1) {
        perror("super block read");
        exit(1);
    }
}

void help(const char *prog) {
    fprintf(stderr, "%s type[oslbid] [, arg1 [, arg2]]\n", prog);
    fprintf(stderr, " o: print disk layout\n");
    fprintf(stderr, " s: print superblock\n");
    fprintf(stderr, " l: print log block, arg1: 終了ログブロック番号\n");
    fprintf(stderr, " b: print bitmap\n");
    fprintf(stderr, " i: print inode, arg1: 開始inum[. arg2: 終了inum[=開始inum]]\n");
    fprintf(stderr, " d: print data, arg1: 開始セクタ番号, [arg2: セクタ数[=1]]\n");
}

void dump_hex(int start, char *buf, int len) {
    int i, j, idx, pos = start;
    for (i = 0; i < len / 16; i++) {
        printf("%08x: ", pos);
        for (j = 0; j < 16; j += 2) {
            idx = i * 16 + j;
            printf("%02x%02x ", (unsigned char)buf[idx], (unsigned char)buf[idx+1]);
        }
        for (j = 0; j < 16; j++) {
            unsigned char c = buf[i*16+j];
            if (c >= 0x20 && c <= 0x7e)
                printf("%c", c);
            else
                printf(".");
        }
        printf("\n");
        pos += 16;
    }
}

void dump(FILE *f, char type, int arg1, int arg2) {

    int seek, iseek, i, j;
    size_t n;
    char buf[BUFSIZE];
    uint32_t v6_addr = mbr.pentries[1].lba * SECTOR_SIZE;

    switch(type) {
    case 's':
        seek = v6_addr + BSIZE;
        printf("SUPER BLOCK:\n");
        printf(" magic     : %x\n", sb.magic);
        printf(" size      : %d (0x%08x)\n", sb.size, sb.size);
        printf(" nblocks   : %d (0x%08x)\n", sb.nblocks, sb.nblocks);
        printf(" ninode    : %d (0x%08x)\n", sb.ninodes, sb.ninodes);
        printf(" nlog      : %d (0x%08x)\n", sb.nlog, sb.nlog);
        printf(" logstart  : %d (0x%08x)\n", sb.logstart, sb.logstart);
        printf(" inodestart: %d (0x%08x)\n", sb.inodestart, sb.inodestart);
        printf(" bmapstart : %d (0x%08x)\n", sb.bmapstart, sb.bmapstart);
        break;
    case 'l':
        for (i = 0; i < arg1; i++) {
            seek = v6_addr + BSIZE * (2 + i);
            fseek(f, seek, SEEK_SET);
            n = fread(buf, sizeof(char), BSIZE, f);
            if (n != BSIZE) {
                printf("data read error: n=%zu", n);
                exit(1);
            }
            printf("LOG: %d (0x%08x)\n", i, data_start + 2 + i);
            dump_hex(seek, buf, 1024);
        }

        break;
    case 'i':
        seek = v6_addr + BSIZE * sb.inodestart;
        for (i = arg1; i <= arg2; i++) {
            iseek = seek + sizeof(struct dinode) * i;
            fseek(f, iseek, SEEK_SET);
            n = fread(buf, sizeof(struct dinode), 1, f);
            if (n != 1) {
                perror("inode read");
                 exit(1);
            }
            struct dinode *inode = (struct dinode *)buf;
            printf("INODE[%d]: 0x%08x\n", i, iseek);
            printf(" type : 0x%04x (%s)\n", inode->type, TYPE_NAME[inode->type]);
            printf(" major: 0x%04x\n", inode->major);
            printf(" minor: 0x%04x\n", inode->minor);
            printf(" nlink: 0x%04x\n", inode->nlink);
            printf(" size : %d (0x%08x)\n", inode->size, inode->size);
            for (j = 0; j < NDIRECT+1; j++) {
                if (inode->addrs[j] == 0) break;
                printf(" addrs[%02d]: %d (0x%08x)\n", j, inode->addrs[j], data_start + inode->addrs[j]);
            }
            printf("\n");
        }
        break;
    case 'b':
        seek = v6_addr + BSIZE * sb.bmapstart;
        fseek(f, seek, SEEK_SET);
        n = fread(buf, sizeof(char), BSIZE, f);
        if (n != BSIZE) {
            perror("bitmap read");
            exit(1);
        }
        printf("BITMAP:\n");
        dump_hex(seek, buf, 128);
        break;
    case 'd':
        for (i = 0; i < arg2; i++) {
            seek = v6_addr + BSIZE * (arg1 + i);
            fseek(f, seek, SEEK_SET);
            n = fread(buf, sizeof(char), BSIZE, f);
            if (n != BSIZE) {
                printf("data read error: n=%zu", n);
                exit(1);
            }
            printf("SECTOR: %d (0x%08x)\n", arg1 + i, data_start + arg1 + i);
            dump_hex(seek, buf, 512);
        }
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[]) {
    char type;
    int arg1, arg2;
    FILE *sdimg;

    // type [s, l, i, b], number
    if (argc < 2) {
        help(argv[0]);
        return 1;
    }

    type = *argv[1];
    if (type == 'i' || type == 'l') {
        if (argc < 3) {
            help(argv[0]);
            return 1;
        } else {
            arg1 = arg2 = atoi(argv[2]);
            if (argc == 4)
                arg2 = atoi(argv[3]);
        }
    }

    if (type == 'd') {
        if (argc < 3) {
            help(argv[0]);
            return 1;
        } else {
            arg1 = atoi(argv[2]);
            if (argc == 4)
                arg2 = atoi(argv[3]);
            else
                arg2 = 1;
        }
    }

    if ((sdimg = fopen("milkv-duo_sdcard.img", "r")) == NULL) {
        perror("open milkv-duo_sdcard.img");
        return 1;
    }

    get_superblock(sdimg);

    uint32_t lba = mbr.pentries[1].lba;
    uint32_t v6_addr = lba * SECTOR_SIZE;
    data_start = mbr.pentries[1].lba + nmeta;

    if (type == 'o') {
        printf("Block     : LBA     Address    (#block)\n");
        printf("Boot      : 0x%x 0x%08x (%d)\n", lba, v6_addr, 1);
        printf("SuperBlock: 0x%x 0x%08x (%d)\n", lba + 8 * 1, v6_addr + BSIZE, 1);
        printf("Log       : 0x%x 0x%08x (%d)\n", lba + 8 * 2, v6_addr + BSIZE * 2, sb.nlog);
        printf("INode     : 0x%x 0x%08x (%d)\n", lba + 8 * sb.inodestart, v6_addr + BSIZE * sb.inodestart, ninode);
        printf("Bitmap    : 0x%x 0x%08x (%d)\n", lba + 8 * sb.bmapstart, v6_addr + BSIZE * sb.bmapstart, nbitmap);
        printf("Data      : 0x%x 0x%08x\n\n", lba + 8 * (sb.bmapstart + nbitmap), v6_addr + BSIZE * (sb.bmapstart + nbitmap));
    } else {
         dump(sdimg, type, arg1, arg2);
    }

    fclose(sdimg);

    return 0;
}
