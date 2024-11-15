#ifndef INC_COMMON_FILE_H
#define INC_COMMON_FILE_H

#include <common/fs.h>
#include <sleeplock.h>

struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int ref;            // reference count
    char readable;
    char writable;
    struct pipe *pipe;  // FD_PIPE
    struct inode *ip;   // FD_INODE and FD_DEVICE
    uint32_t off;       // FD_INODE
    short major;        // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode
struct inode {
    uint32_t dev;           // Device number
    uint32_t inum;          // Inode number
    int ref;                // Reference count
    struct sleeplock lock;  // protects everything below here
    int valid;              // inode has been read from disk?
    short type;             // copy of disk inode
    short major;
    short minor;
    short nlink;
    uint32_t size;
    uint32_t addrs[NDIRECT+1];
};

// map major device number to device functions.
struct devsw {
    int (*read)(int, uint64_t, int);
    int (*write)(int, uint64_t, int);
    int (*ioctl)(int, unsigned long, void *);
};

extern struct devsw devsw[];

#define CONSOLE 1
#define GPIO 2
#define PWM 3
#define ADC 4
#define I2C 5
#define SPI 6

#endif
