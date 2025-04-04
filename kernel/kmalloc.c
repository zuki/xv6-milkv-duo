#include <common/types.h>
#include <defs.h>
#include <common/riscv.h>
#include <printf.h>

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.    Section 8.7.

typedef __uint128_t Align;

union header {
    struct {
        union header *ptr;
        uint64_t size;
    } s;
    Align x;
};

typedef union header Header;

static Header base;
static Header *freep = NULL;

void kmfree(void *ap)
{
    Header *bp, *p;

    bp = (Header*)ap - 1;
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    if (bp + bp->s.size == p->s.ptr){
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    } else
        bp->s.ptr = p->s.ptr;
    if (p + p->s.size == bp){
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    } else
        p->s.ptr = bp;
    freep = p;
}

//use kalloc instead of growproc to allocate memory
//to kernel data structures.
//kalloc always returns PGSIZE memory on success
//size parameter of the Header takes Header sized chunks
static Header*
morecore()
{
    char *p;
    Header *hp;

    p = kalloc();
    if (p == 0)
        return NULL;
    memset(p, 0, PGSIZE);
    hp = (Header*)p;
    hp->s.size = PGSIZE / sizeof(Header);
    kmfree((void*)(hp + 1));
    return freep;
}

void*
kmalloc(size_t nbytes)
{
    Header *p, *prevp;
    uint64_t nunits;

    // panic if we try to allocate more than 4080
    if (nbytes > (PGSIZE - sizeof(Header))) {
        error("req more than 4080: %ld", nbytes);
        panic("too big request");
    }

    nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;

    if((prevp = freep) == 0){
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }

    for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
        if (p->s.size >= nunits){
            if (p->s.size == nunits)
                prevp->s.ptr = p->s.ptr;
            else {
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void*)(p + 1);
        }
        if (p == freep)
            if ((p = morecore()) == 0)
                return NULL;
    }
}
