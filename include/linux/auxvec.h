/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef INC_LINUX_AUXVEC_H
#define INC_LINUX_AUXVEC_H

/* Symbolic values for the entries in the auxiliary table
   put on the initial stack */
#define AT_NULL   0	/* end of vector */
#define AT_IGNORE 1	/* entry should be ignored */
#define AT_EXECFD 2	/* file descriptor of program */
#define AT_PHDR   3	/* program headers for program */
#define AT_PHENT  4	/* size of program header entry */
#define AT_PHNUM  5	/* number of program headers */
#define AT_PAGESZ 6	/* system page size */
#define AT_BASE   7	/* base address of interpreter */
#define AT_FLAGS  8	/* flags */
#define AT_ENTRY  9	/* entry point of program */
#define AT_NOTELF 10	/* program is not ELF */
#define AT_UID    11	/* real uid */
#define AT_EUID   12	/* effective uid */
#define AT_GID    13	/* real gid */
#define AT_EGID   14	/* effective gid */
#define AT_PLATFORM 15  /* string identifying CPU for optimizations */
#define AT_HWCAP  16    /* arch dependent hints at CPU capabilities */
#define AT_CLKTCK 17	/* frequency at which times() increments */
/* AT_* values 18 through 22 are reserved */
#define AT_SECURE 23   /* secure mode boolean */
#define AT_BASE_PLATFORM 24	/* string identifying real platform, may
				 * differ from AT_PLATFORM. */
#define AT_RANDOM 25	/* address of 16 random bytes */
#define AT_HWCAP2 26	/* extension of AT_HWCAP */

#define AT_EXECFN  31	/* filename of program */

/* from arch/riscv/include/upai/asm/auxvec.h */

/* vDSO location */
#define AT_SYSINFO_EHDR 33

/*
 * The set of entries below represent more extensive information
 * about the caches, in the form of two entry per cache type,
 * one entry containing the cache size in bytes, and the other
 * containing the cache line size in bytes in the bottom 16 bits
 * and the cache associativity in the next 16 bits.
 *
 * The associativity is such that if N is the 16-bit value, the
 * cache is N way set associative. A value if 0xffff means fully
 * associative, a value of 1 means directly mapped.
 *
 * For all these fields, a value of 0 means that the information
 * is not known.
 */
#define AT_L1I_CACHESIZE    40
#define AT_L1I_CACHEGEOMETRY    41
#define AT_L1D_CACHESIZE    42
#define AT_L1D_CACHEGEOMETRY    43
#define AT_L2_CACHESIZE     44
#define AT_L2_CACHEGEOMETRY 45
#define AT_L3_CACHESIZE     46
#define AT_L3_CACHEGEOMETRY 47

/* entries in ARCH_DLINFO */
#define AT_VECTOR_SIZE_ARCH 9

#endif /* INC_LINUX_AUXVEC_H */
