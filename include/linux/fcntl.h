#ifndef INC_LINUX_FCNTL_H
#define INC_LINUX_FCNTL_H

#define O_SEARCH   O_PATH
#define O_EXEC     O_PATH
#define O_TTY_INIT 0

#define O_ACCMODE (03|O_SEARCH)
#define O_RDONLY  00
#define O_WRONLY  01
#define O_RDWR    02

#define O_CREAT        0100     //     0x40
#define O_EXCL         0200     //     0x80
#define O_NOCTTY       0400     //    0x100
#define O_TRUNC       01000     //    0x200
#define O_APPEND      02000     //    0x400
#define O_NONBLOCK    04000     //    0x800
#define O_DSYNC      010000     //   0x1000
#define O_ASYNC      020000     //   0x2000
#define O_DIRECTORY  040000     //   0x4000
#define O_NOFOLLOW  0100000     //   0x8000
#define O_DIRECT    0200000     //  0x10000
#define O_LARGEFILE 0400000     //  0x20000
#define O_NOATIME  01000000     //  0x40000
#define O_CLOEXEC  02000000     //  0x80000
#define O_SYNC     04010000     // 0x101000
#define O_RSYNC    04010000     // 0x101000
#define O_PATH    010000000     // 0x200000
#define O_TMPFILE 020040000     // 0x404000
#define O_NDELAY O_NONBLOCK

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_ULOCK 0
#define F_LOCK  1
#define F_TLOCK 2
#define F_TEST  3

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  5
#define F_SETLK  6
#define F_SETLKW 7
#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11

#define F_SETOWN_EX 15
#define F_GETOWN_EX 16

#define F_GETOWNER_UIDS 17

#define FD_CLOEXEC 1

#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_FOLLOW 0x400
#define AT_EACCESS 0x200
#define AT_NO_AUTOMOUNT 0x800
#define AT_EMPTY_PATH 0x1000
#define AT_STATX_SYNC_TYPE 0x6000
#define AT_STATX_SYNC_AS_STAT 0x0000
#define AT_STATX_FORCE_SYNC 0x2000
#define AT_STATX_DONT_SYNC 0x4000
#define AT_RECURSIVE 0x8000

#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_RANDOM     1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED   3
#ifndef POSIX_FADV_DONTNEED
#define POSIX_FADV_DONTNEED   4
#define POSIX_FADV_NOREUSE    5
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2


#endif
