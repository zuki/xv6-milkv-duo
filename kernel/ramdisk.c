//
// ramdisk that uses the disk image loaded by qemu -initrd fs.img
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

extern char __ramdisk_start[];

void
ramdiskinit(void)
{
}

void
ramdiskrw(struct buf *b, int write)
{
  if(!holdingsleep(&b->lock))
    panic("ramdiskrw: buf not locked");

  if(b->blockno >= FSSIZE)
    panic("ramdiskrw: blockno too big");

  uint64 diskaddr = b->blockno * BSIZE;
  char *addr = (char *)__ramdisk_start + diskaddr;

  if(write){
    // write
    memmove(addr, b->data, BSIZE);
  } else {
    // read
    memmove(b->data, addr, BSIZE);
  }
}
