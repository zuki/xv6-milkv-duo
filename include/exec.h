#ifndef INC_EXEC_H
#define INC_EXEC_H

#include <common/types.h>
#include <common/memlayout.h>
#include <elf.h>
#include <linux/capability.h>

#define EM_RISCV            243
#define ELF_ARCH            EM_RISCV
#define ELF_CLASS           ELFCLASS64
#define ELF_DATA            ELFDATA2LSB

#define ELF_MIN_ALIGN       4096
#define ELF_PAGESTART(_v)   ((_v) & ~(uint64_t)(ELF_MIN_ALIGN-1))
#define ELF_PAGEOFFSET(_v)  ((_v) & (uint64_t)(ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v)   (((_v) + ELF_MIN_ALIGN - 1) & ~(uint64_t)(ELF_MIN_ALIGN - 1))
// ET_DYNプログラムが実行された場合にロードされる場所である。
// 典型的な使い方は、新しいバージョンのローダーをテストするために
// "./ld.so someprog"を呼び出すことである。 ldが"exec"するプログラムの
// 邪魔にならないようにし、brkのための十分なスペースがあることを
// 確認する必要がある。
#define ELF_EXEC_PAGESIZE   4096
#define BINPRM_BUF_SIZE     128

#define BAD_ADDR(x)         ((uint64_t)(x) > USERTOP)
// AUXVの個数
#define ELF_CHECK_ARCH(x)   ((x)->machine == EM_RISCV)

// 以下の値はriscv,isa = "rv64imafdvcsu"から作成(hwcap.c)
// ELF capabilities acdfimv
#define ELF_HWCAP           0x20112d
#define ELF_HWCAP2          0
#define ELF_PLATFORM        "riscv"

#ifndef elf_addr_t
#define elf_addr_t          uint64_t
#define elf_caddr_t         char *
#endif

#endif
