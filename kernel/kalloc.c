// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

//#include <common/types.h>
//#include <common/param.h>
//#include <common/memlayout.h>
//#include "spinlock.h"
//#include <common/riscv.h>
#include "defs.h"
#include "page.h"
#include "common/riscv.h"
#include "printf.h"

#if 0
void freerange(void *pa_start, void *pa_end);

extern char _end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(_end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64_t)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}
#endif

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
#if 0
    struct run *r;

    if(((uint64_t)pa % PGSIZE) != 0 || (char*)pa < _end || (uint64_t)pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
#endif
    buddy_free(page_find_by_address(pa));
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void)
{
#if 0
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if(r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
#endif
    struct page *page = buddy_alloc(PGSIZE);
    // return page_address(page);
    void *addr = page_address(page);
    //debug("addr: 0x%08x\n", addr);
    return addr;
}
