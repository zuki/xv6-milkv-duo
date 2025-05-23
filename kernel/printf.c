//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include <common/types.h>
#include <common/param.h>
#include "spinlock.h"
#include "sleeplock.h"
#include <common/fs.h>
#include <common/file.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include "defs.h"
#include "proc.h"
#include "printf.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
    struct spinlock lock;
    int locking;
} pr;

#define putchar consputc
#define get_num_va_args(_args, _lcount)                \
    (((_lcount) > 1)  ? va_arg(_args, long long int) :    \
    (((_lcount) == 1) ? va_arg(_args, long int) :        \
                va_arg(_args, int)))

#define get_unum_va_args(_args, _lcount)                \
    (((_lcount) > 1)  ? va_arg(_args, unsigned long long int) :    \
    (((_lcount) == 1) ? va_arg(_args, unsigned long int) :        \
                va_arg(_args, unsigned int)))

static int string_print(const char *str)
{
    int count = 0;

    //assert(str != NULL);

    for ( ; *str != '\0'; str++) {
        (void)putchar(*str);
        count++;
    }

    return count;
}

static int unsigned_num_print(unsigned long int unum, unsigned int radix,
                  char padc, int padn)
{
    /* Just need enough space to store 64 bit decimal integer */
    char num_buf[100];
    int i = 0, count = 0, bc=0;
    unsigned int rem;

    do {
        rem = unum % radix;
        if (rem < 0xa)
            num_buf[i] = '0' + rem;
        else
            num_buf[i] = 'a' + (rem - 0xa);
        i++;
        if (radix == 2) {
            if (bc % 4 == 0 && bc != 0) {
                num_buf[i] = '_';
                i++;
                bc++;
            }
        }
        unum /= radix;
    } while (unum > 0U);

    if (padn > 0) {
        if (radix == 2) {
            while (bc < padn) {
                (void)putchar(padc);
                count++;
                padn--;
                if (padn % 4 == 0) {
                    (void)putchar('_');
                }
            }
        } else {
            while (i < padn) {
                (void)putchar(padc);
                count++;
                padn--;
            }
        }
    }

    while (--i >= 0) {
        (void)putchar(num_buf[i]);
        count++;
    }

    return count;
}

/*******************************************************************
 * Reduced format print for Trusted firmware.
 * The following type specifiers are supported by this print
 * %x - hexadecimal format
 * %s - string format
 * %d or %i - signed decimal format
 * %u - unsigned decimal format
 * %p - pointer format
 *
 * The following length specifiers are supported by this print
 * %l - long int (64-bit on AArch64)
 * %ll - long long int (64-bit on AArch64)
 * %z - size_t sized integer formats (64 bit on AArch64)
 *
 * The following padding specifiers are supported by this print
 * %0NN - Left-pad the number with 0s (NN is a decimal number)
 *
 * The print exits on all other formats specifiers other than valid
 * combinations of the above specifiers.
 *******************************************************************/
int vprintf(const char *fmt, va_list args)
{
    int l_count;
    long long int num;
    unsigned long long int unum;
    char *str;
    char padc = '\0'; /* Padding character */
    int padn; /* Number of characters to pad */
    int count = 0; /* Number of printed characters */

    while (*fmt != '\0') {
        l_count = 0;
        padn = 0;

        if (*fmt == '%') {
            fmt++;
            /* Check the format specifier */
loop:
            switch (*fmt) {
            case '%':
                (void)putchar('%');
                break;
            case 'i': /* Fall through to next one */
            case 'd':
                num = get_num_va_args(args, l_count);
                if (num < 0) {
                    (void)putchar('-');
                    unum = (unsigned long long int)-num;
                    padn--;
                } else
                    unum = (unsigned long long int)num;

                count += unsigned_num_print(unum, 10,
                                padc, padn);
                break;
            case 's':
                str = va_arg(args, char *);
                count += string_print(str);
                break;
            case 'p':
                unum = (uintptr_t)va_arg(args, void *);
                if (unum > 0U) {
                    count += string_print("0x");
                    padn -= 2;
                }

                count += unsigned_num_print(unum, 16,
                                padc, padn);
                break;
            case 'x':
                unum = get_unum_va_args(args, l_count);
                count += unsigned_num_print(unum, 16,
                                padc, padn);
                break;
            case 'b':
                unum = get_unum_va_args(args, l_count);
                count += unsigned_num_print(unum, 2,
                                padc, padn);
                break;
            case 'z':
                if (sizeof(size_t) == 8U)
                    l_count = 2;

                fmt++;
                goto loop;
            case 'l':
                l_count++;
                fmt++;
                goto loop;
            case 'u':
                unum = get_unum_va_args(args, l_count);
                count += unsigned_num_print(unum, 10,
                                padc, padn);
                break;
            case '0':
            case ' ':
                padc = *fmt;
                padn = 0;
                fmt++;

                for (;;) {
                    char ch = *fmt;
                    if ((ch < '0') || (ch > '9')) {
                        goto loop;
                    }
                    padn = (padn * 10) + (ch - '0');
                    fmt++;
                }
                //assert(0); /* Unreachable */
            default:
                /* Exit on any other format specifier */
                return -1;
            }
            fmt++;
            continue;
        }
        (void)putchar(*fmt);
        fmt++;
        count++;
    }

    return count;
}

int printf(const char *fmt, ...)
{
    int count, locking;
    va_list va;

    locking = pr.locking;
    if (locking)
        acquire(&pr.lock);

    va_start(va, fmt);
    count = vprintf(fmt, va);
    va_end(va);

    if (locking)
        release(&pr.lock);

    return count;
}

void panic(char *s)
{
    pr.locking = 0;
    printf("panic: ");
    printf(s);
    printf("\n");
    panicked = 1; // freeze uart output from other CPUs
    for(;;)
        ;
}

void printfinit(void)
{
    initlock(&pr.lock, "pr");
    pr.locking = 1;
}

void debug_bytes(char *title, char *buf, int size) {
    int lines = (size / 16) + 1;
    if (size % 16 == 0) lines -= 1;

    uint64_t byte = 0;
    printf("%s\n", title);
    for (int i = 0; i < lines; i++) {
        for (int j = 0; j < 16; j++) {
            if (j == 0)
                printf("%016x:", (uint64_t)buf + byte);
            if (j%2)
                printf("%02x", buf[i*16+j]);
            else
                printf(" %02x", buf[i*16+j]);
      }
      printf("\n");
      byte += 16;
    }
}
