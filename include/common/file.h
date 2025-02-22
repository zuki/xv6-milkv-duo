#ifndef INC_COMMON_FILE_H
#define INC_COMMON_FILE_H

#include <common/fs.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <sleeplock.h>

struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int ref;            // reference count
    struct pipe *pipe;  // FD_PIPE
    struct inode *ip;   // FD_INODE and FD_DEVICE
    uint32_t off;       // FD_INODE
    int flags;          //
    short major;        // FD_DEVICE
    char readable;
    char writable;
};

#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

#define FILE_STATUS_FLAGS (O_APPEND|O_ASYNC|O_DIRECT|O_DSYNC|O_NOATIME|O_NONBLOCK|O_SYNC)
#define FILE_READABLE(flags) ((((flags) & O_ACCMODE) == O_RDWR) || (((flags) & O_ACCMODE) == O_RDONLY));
#define FILE_WRITABLE(flags) ((((flags) & O_ACCMODE) == O_RDWR) || (((flags) & O_ACCMODE) == O_WRONLY));


// in-memory copy of an inode
struct inode {
    uint32_t dev;               // Device number
    uint32_t inum;              // Inode number
    int ref;                    // Reference count
    struct sleeplock lock;      // protects everything below here
    int valid;                  // inode has been read from disk?
    short type;                 // copy of disk inode
    short major;                // メジャーデバイス番号 (T_DEVICE only)
    short minor;                // マイナーデバイス番号 (T_DEVICE only)
    short nlink;                // inodeへのリンク数
    uint32_t size;              // ファイルサイズ（バイト単位）(bytes)
    mode_t mode;                // ファイルモード
    uid_t uid;                  // 所有者のユーザーID
    gid_t gid;                  // 所有者のグループID
    struct timespec atime;      // 最新アクセス日時
    struct timespec mtime;      // 最新更新日時
    struct timespec ctime;      // 作成日時
    uint32_t addrs[NDIRECT+2];  // データブロックのアドレス
};

// map major device number to device functions.
struct devsw {
    int (*read)(int, uint64_t, int);
    int (*write)(int, uint64_t, int);
    int (*ioctl)(int, uint64_t, void *);
};

extern struct devsw devsw[];

#define CONSOLE 1
#define GPIO 2
#define PWM 3
#define ADC 4
#define I2C 5
#define SPI 6

#endif
