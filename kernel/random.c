#include <types.h>
#include <riscv.h>
#include <defs.h>
#include <errno.h>
#include <proc.h>
#include <spinlock.h>
#include <cv181x_reg.h>
#include <riscv-mmio.h>

struct spinlock randomlock;

static void get_rng(uint32_t *rng)
{
    while (mmio_read_32(TRNG_STAT) & TRNG_STAT_BUSY) {}
    mmio_write_32(TRNG_CTRL, TRNG_CMD_GEN_RANDOM);
    while ((mmio_read_32(TRNG_ISTAT) & TRNG_DONE_R1) == 0) {}
    mmio_write_32(TRNG_ISTAT, TRNG_DONE_W1);
    rng[0] = mmio_read_32(TRNG_RAND0);
    rng[1] = mmio_read_32(TRNG_RAND1);
    rng[2] = mmio_read_32(TRNG_RAND2);
    rng[3] = mmio_read_32(TRNG_RAND3);
}

void randinit(void)
{
    initlock(&randomlock, "random");

    while (mmio_read_32(TRNG_STAT) & TRNG_STAT_BUSY) {}
    mmio_write_32(TRNG_CTRL, TRNG_CMD_GEN_NOISE);
    while ((mmio_read_32(TRNG_ISTAT) & TRNG_DONE_R1) == 0) {}
    mmio_write_32(TRNG_ISTAT, TRNG_DONE_W1);
    mmio_write_32(TRNG_CTRL, TRNG_CMD_CREATE_STATE);
    while ((mmio_read_32(TRNG_ISTAT) & TRNG_DONE_R1) == 0) {}
    mmio_write_32(TRNG_ISTAT, TRNG_DONE_W1);
}

long getrandom(uint64_t bufp, size_t buflen)
{
    uint32_t max_words = (buflen + 3 ) / sizeof(uint32_t);
    int count = 0;
    uint32_t buf[max_words];
    uint32_t rng[4];

    acquire(&randomlock);
    while(1) {
        get_rng(rng);
        for (int k = 0; k < 4; k++) {
            if (count + k < max_words) {
                ((uint32_t *)buf)[count+k] = rng[k];
            } else {
                goto cont;
            }
        }
        count += 4;
    }
cont:
    release(&randomlock);

    if (copyout(myproc()->pagetable, bufp, (char *)&buf, max_words * sizeof(uint32_t)) < 0)
        return -EFAULT;

    return max_words * sizeof(uint32_t);
}
