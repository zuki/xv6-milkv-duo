#include <stdio.h>
#include <string.h>

#define COMPAT_HWCAP_ISA_I      (1 << ('I' - 'A'))
#define COMPAT_HWCAP_ISA_M      (1 << ('M' - 'A'))
#define COMPAT_HWCAP_ISA_A      (1 << ('A' - 'A'))
#define COMPAT_HWCAP_ISA_F      (1 << ('F' - 'A'))
#define COMPAT_HWCAP_ISA_D      (1 << ('D' - 'A'))
#define COMPAT_HWCAP_ISA_C      (1 << ('C' - 'A'))
#define COMPAT_HWCAP_ISA_V      (1 << ('V' - 'A'))

void main(void)
{
    const char *isa = "rv64imafdvcsu";
    char print_str[65];
    size_t i, j, isa_len;
    static unsigned long isa2hwcap[256] = {0};

    isa2hwcap['i'] = isa2hwcap['I'] = COMPAT_HWCAP_ISA_I;
    isa2hwcap['m'] = isa2hwcap['M'] = COMPAT_HWCAP_ISA_M;
    isa2hwcap['a'] = isa2hwcap['A'] = COMPAT_HWCAP_ISA_A;
    isa2hwcap['f'] = isa2hwcap['F'] = COMPAT_HWCAP_ISA_F;
    isa2hwcap['d'] = isa2hwcap['D'] = COMPAT_HWCAP_ISA_D;
    isa2hwcap['c'] = isa2hwcap['C'] = COMPAT_HWCAP_ISA_C;
    isa2hwcap['v'] = isa2hwcap['V'] = COMPAT_HWCAP_ISA_V;

    unsigned long elf_hwcap = 0;

    i = 0;
    isa_len = strlen(isa);
    if (!strncmp(isa, "rv64", 4))
        i += 4;

    for (; i < isa_len; ++i) {
        elf_hwcap |= isa2hwcap[(unsigned char)(isa[i])];
    }

    printf("hwcap = 0x%lx\n", elf_hwcap);

    memset(print_str, 0, sizeof(print_str));
    for (i = 0, j = 0; i < 64; i++)
        if (elf_hwcap & (1UL << i))
            print_str[j++] = (char)('a' + i);
    printf("riscv: ELF capabilities %s\n", print_str);
}
