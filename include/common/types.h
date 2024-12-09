#ifndef INC_TYPES_H
#define INC_TYPES_H

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H
#endif

typedef unsigned int    uint;
typedef unsigned short  ushort;
typedef unsigned char   uchar;

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
typedef unsigned long   uint64_t;
typedef unsigned long   uintptr_t;
typedef unsigned long   size_t;

typedef short       int16_t;
typedef int         int32_t;
typedef long        int64_t;
typedef long        ssize_t;

typedef uint64_t    pde_t;
typedef int         boot;

typedef uint64_t    dma_addr_t;

typedef int64_t         ssize_t;
typedef uint64_t        size_t;
typedef uint64_t        dev_t;
typedef uint64_t        ino_t;
typedef uint32_t        mode_t;
typedef uint32_t        nlink_t;
typedef int32_t         pid_t;
typedef uint32_t        uid_t;
typedef uint32_t        gid_t;
typedef uint64_t        off_t;
typedef int32_t         blksize_t;
typedef int64_t         blkcnt_t;
typedef int64_t         time_t;
typedef int64_t         suseconds_t;
typedef uint32_t        tcflag_t;
typedef uint32_t        speed_t;
typedef uint8_t         cc_t;
typedef int32_t         clockid_t;
typedef uint64_t        ino64_t;
typedef off_t           off64_t;
typedef uint32_t        kernel_cap_t;
typedef uint64_t        uintptr_t;
typedef int64_t         intptr_t;
typedef unsigned        wint_t;
typedef uint16_t        kdev_t;
typedef uint64_t        handler_t;

typedef uint32_t    __be32;

typedef int         bool;
#define true        1
#define false       0

#define NULL        ((void *)0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define uswap_16(x) \
	((((x) & 0xff00) >> 8) | \
	 (((x) & 0x00ff) << 8))
#define uswap_32(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))
#define _uswap_64(x, sfx) \
	((((x) & 0xff00000000000000##sfx) >> 56) | \
	 (((x) & 0x00ff000000000000##sfx) >> 40) | \
	 (((x) & 0x0000ff0000000000##sfx) >> 24) | \
	 (((x) & 0x000000ff00000000##sfx) >>  8) | \
	 (((x) & 0x00000000ff000000##sfx) <<  8) | \
	 (((x) & 0x0000000000ff0000##sfx) << 24) | \
	 (((x) & 0x000000000000ff00##sfx) << 40) | \
	 (((x) & 0x00000000000000ff##sfx) << 56))
#if defined(__GNUC__)
# define uswap_64(x) _uswap_64(x, ull)
#else
# define uswap_64(x) _uswap_64(x, )
#endif

# define cpu_to_le16(x)		(x)
# define cpu_to_le32(x)		(x)
# define cpu_to_le64(x)		(x)
# define le16_to_cpu(x)		(x)
# define le32_to_cpu(x)		(x)
# define le64_to_cpu(x)		(x)
# define cpu_to_be16(x)		uswap_16(x)
# define cpu_to_be32(x)		uswap_32(x)
# define cpu_to_be64(x)		uswap_64(x)
# define be16_to_cpu(x)		uswap_16(x)
# define be32_to_cpu(x)		uswap_32(x)
# define be64_to_cpu(x)		uswap_64(x)

#define bit_add(p, n)       ((p) |= (1U << (n)))
#define bit_remove(p, n)    ((p) &= ~(1U << (n)))
#define bit_test(p, n)      ((p) & (1U << (n)))

/* Efficient min and max operations */
#define MIN(_a, _b)                 \
({                                  \
    typeof(_a) __a = (_a);          \
    typeof(_b) __b = (_b);          \
    __a <= __b ? __a : __b;         \
})
#define MAX(_a, _b)                 \
({                                  \
    typeof(_a) __a = (_a);          \
    typeof(_b) __b = (_b);          \
    __a >= __b ? __a : __b;         \
})

#define ALIGN(p, n) (((p) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

#endif
