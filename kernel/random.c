// from https://kobayashi.hub.hit-u.ac.jp/topics/rand.html

#include <types.h>
#include <riscv.h>
#include <defs.h>
#include <errno.h>
#include <proc.h>
#include <spinlock.h>

struct spinlock randomlock;

void randinit(void)
{
    initlock(&randomlock, "random");
}

uint32_t xorshift(void){
    static uint32_t x = 123456789;
    static uint32_t y = 362436069;
    static uint32_t z = 521288629;
    static uint32_t w = 88675123;
    uint32_t t;
    t = x ^ (x<<11);
    x = y; y = z; z = w;
    w ^= t ^ (t>>8) ^ (w>>19);
    return w;
}

long getrandom(uint64_t bufp, size_t buflen)
{
    uint32_t max_words = (buflen + 3 ) / sizeof(uint32_t);
    uint32_t count;
    uint32_t buf[max_words];

    acquire(&randomlock);
    for (count = 0; count < max_words; count++)
        ((uint32_t *)buf)[count] = xorshift();
    release(&randomlock);

    if (copyout(myproc()->pagetable, bufp, (char *)&buf, max_words * sizeof(uint32_t)) < 0)
        return -EFAULT;

    return max_words * sizeof(uint32_t);
}
