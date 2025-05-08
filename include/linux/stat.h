#ifndef INC_LINUX_STAT_H
#define INC_LINUX_STAT_H

#include <common/types.h>
#include <linux/time.h>

#define S_IFMT   00170000

#define S_IFSOCK 0140000        // 0xc000
#define S_IFLNK  0120000        // 0xa000
#define S_IFREG  0100000        // 0x8000
#define S_IFBLK  0060000        // 0x6000
#define S_IFDIR  0040000        // 0x4000
#define S_IFCHR  0020000        // 0x2000
#define S_IFIFO  0010000        // 0x1000
#define S_ISUID  0004000        //  0x800
#define S_ISGID  0002000        //  0x400
#define S_ISVTX  0001000        //  0x200
#define S_IRUSR  0400           //  0x100
#define S_IWUSR  0200           //   0x80
#define S_IXUSR  0100           //   0x40
#define S_IRWXU  0700
#define S_IRGRP  0040           //   0x20
#define S_IWGRP  0020           //   0x10
#define S_IXGRP  0010           //    0x8
#define S_IRWXG  0070
#define S_IROTH  0004           //    0x4
#define S_IWOTH  0002           //    0x2
#define S_IXOTH  0001           //    0x1
#define S_IRWXO  0007

#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
#define S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

#define S_IRWXUGO   (S_IRWXU|S_IRWXG|S_IRWXO)
#define S_IALLUGO   (S_ISUID|S_ISGID|S_ISVTX|S_IRWXUGO)
#define S_IRUGO     (S_IRUSR|S_IRGRP|S_IROTH)
#define S_IWUGO     (S_IWUSR|S_IWGRP|S_IWOTH)
#define S_IXUGO     (S_IXUSR|S_IXGRP|S_IXOTH)

#define UTIME_NOW  0x3fffffff
#define UTIME_OMIT 0x3ffffffe

#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device
#define T_MOUNT     4   // Mount Point
#define T_SYMLINK   5   // Sysbolic link

#define DT_UNKNOWN      0
#define DT_FIFO         1
#define DT_CHR          2
#define DT_DIR          4
#define DT_BLK          6
#define DT_REG          8
#define DT_LNK          10
#define DT_SOCK         12
#define DT_WHT          14
#define IFTODT(x)       ((x)>>12 & 017)
#define DTTOIF(x)       ((x)<<12)

#define STATX_TYPE      1U      // 0
#define STATX_MODE      2U      // 1    x
#define STATX_NLINK     4U      // 2    x   y
#define STATX_UID       8U      // 3    x   y
#define STATX_GID       0x10U   // 4    x   y
#define STATX_ATIME     0x20U   // 5        y
#define STATX_MTIME     0x40U   // 6    x   y
#define STATX_CTIME     0x80U   // 7        y
#define STATX_INO       0x100U  // 8        y
#define STATX_SIZE      0x200U  // 9    x   y
#define STATX_BLOCKS    0x400U  // 10       y
#define STATX_BASIC_STATS   0x7ffU  //
#define STATX_BTIME     0x800U  // 11
#define STATX_ALL           0xfffU
#define STATX_MNT_ID    0x1000U // 12
#define STATX_DIOALIGN  0x2000U // 13
#define STATX_MNT_ID_UNIQUE 0x4000U // 14

#define STATX_ATTR_COMPRESSED 0x4
#define STATX_ATTR_IMMUTABLE 0x10
#define STATX_ATTR_APPEND 0x20
#define STATX_ATTR_NODUMP 0x40
#define STATX_ATTR_ENCRYPTED 0x800
#define STATX_ATTR_AUTOMOUNT 0x1000
#define STATX_ATTR_MOUNT_ROOT 0x2000
#define STATX_ATTR_VERITY 0x100000
#define STATX_ATTR_DAX 0x200000


struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    unsigned long __pad;
    off_t st_size;
    blksize_t st_blksize;
    int __pad2;
    blkcnt_t st_blocks;
    struct timespec st_atime;
    struct timespec st_mtime;
    struct timespec st_ctime;
    unsigned __unused[2];
};


struct statx_timestamp {
    int64_t tv_sec;     /* Seconds since the Epoch (UNIX time) */
    uint32_t tv_nsec;   /* Nanoseconds since tv_sec */
};

struct statx {
    uint32_t stx_mask;        /* Mask of bits indicating
                              filled fields */
    uint32_t stx_blksize;     /* Block size for filesystem I/O */
    uint64_t stx_attributes;  /* Extra file attribute indicators */
    uint32_t stx_nlink;       /* Number of hard links */
    uint32_t stx_uid;         /* User ID of owner */
    uint32_t stx_gid;         /* Group ID of owner */
    uint16_t stx_mode;        /* File type and mode */
    uint64_t stx_ino;         /* Inode number */
    uint64_t stx_size;        /* Total size in bytes */
    uint64_t stx_blocks;      /* Number of 512B blocks allocated */
    uint64_t stx_attributes_mask;
                           /* Mask to show what's supported
                              in stx_attributes */

    /* The following fields are file timestamps */
    struct statx_timestamp stx_atime;  /* Last access */
    struct statx_timestamp stx_btime;  /* Creation */
    struct statx_timestamp stx_ctime;  /* Last status change */
    struct statx_timestamp stx_mtime;  /* Last modification */

    /* If this file represents a device, then the next two
       fields contain the ID of the device */
    uint32_t stx_rdev_major;  /* Major ID */
    uint32_t stx_rdev_minor;  /* Minor ID */

    /* The next two fields contain the ID of the device
       containing the filesystem where the file resides */
    uint32_t stx_dev_major;   /* Major ID */
    uint32_t stx_dev_minor;   /* Minor ID */

    uint64_t stx_mnt_id;      /* Mount ID */

    /* Direct I/O alignment restrictions */
    uint32_t stx_dio_mem_align;
    uint32_t stx_dio_offset_align;

    uint64_t stx_subvol;      /* Subvolume identifier */

    /* Direct I/O atomic write limits */
    uint32_t stx_atomic_write_unit_min;
    uint32_t stx_atomic_write_unit_max;
    uint32_t stx_atomic_write_segments_max;

    /* File offset alignment for direct I/O reads */
    uint32_t   stx_dio_read_offset_align;
};

#endif
