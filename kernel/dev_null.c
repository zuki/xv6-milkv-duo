#include <types.h>
#include <defs.h>
#include <file.h>

static int devnull_read(int user_dst, uint64_t dst, int n)
{
    return 0;
}

static int devnull_write(int user_src, uint64_t src, int n)
{
    return n;
}

void devnull_init(void)
{
    devsw[DEVNULL].read = devnull_read;
    devsw[DEVNULL].write = devnull_write;
    devsw[DEVNULL].ioctl = NULL;
}
