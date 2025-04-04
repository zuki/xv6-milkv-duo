#ifndef INC_ELF_H
#define INC_ELF_H

// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

#define ELFCLASSNONE    0               /* EI_CLASS */
#define ELFCLASS32      1
#define ELFCLASS64      2
#define ELFCLASSNUM     3

#define ELFDATANONE     0               /* e_ident[EI_DATA] */
#define ELFDATA2LSB     1
#define ELFDATA2MSB     2

#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define ET_CORE         4
#define ET_NUM          5
#define ET_LOOS         0xfe00
#define ET_HIOS         0xfeff
#define ET_LOPROC       0xff00
#define ET_HIPROC       0xffff


// File header 20 + 4 + 24 + 4 + 12 = 64
struct elfhdr {
    uint32_t magic;  // must equal ELF_MAGIC
    uchar elf[12];
    ushort type;
    ushort machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    ushort ehsize;
    ushort phentsize;
    ushort phnum;
    ushort shentsize;
    ushort shnum;
    ushort shstrndx;
};

// Program section header
struct proghdr {
    uint32_t type;
    uint32_t flags;
    uint64_t off;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

typedef struct {
    uint32_t	st_name;        // 4
    unsigned char	st_info;    // 1
    unsigned char st_other;     // 1
    uint16_t	st_shndx;       // 2
    uint64_t	st_value;       // 8
    uint64_t	st_size;        // 8
} Sym;                    // 24

#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_SHLIB        5
#define PT_PHDR         6
#define PT_TLS          7
#define PT_NUM          8
#define PT_LOOS         0x60000000
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_GNU_STACK    0x6474e551
#define PT_GNU_RELRO    0x6474e552
#define PT_GNU_PROPERTY 0x6474e553
#define PT_LOSUNW       0x6ffffffa
#define PT_SUNWBSS      0x6ffffffa
#define PT_SUNWSTACK    0x6ffffffb
#define PT_HISUNW       0x6fffffff
#define PT_HIOS         0x6fffffff
#define PT_LOPROC       0x70000000
#define PT_HIPROC       0x7fffffff

#define PF_X            0x1
#define PF_W            0x2
#define PF_R            0x4
#define PF_MASKOS       0x0ff00000
#define PF_MASKPROC     0xf0000000

#endif
