#include <common/types.h>
#include <common/riscv.h>
#include <defs.h>
#include <common/param.h>
#include <pipe.h>
#include <spinlock.h>
#include <proc.h>
#include <common/fs.h>
#include "sleeplock.h"
#include <common/file.h>

int
pipealloc(struct file **f0, struct file **f1, int flags)
{
    struct pipe *pi;

    pi = 0;
    *f0 = *f1 = 0;
    if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
        goto bad;
    if((pi = (struct pipe*)kalloc()) == 0)
        goto bad;
    pi->readopen = 1;
    pi->writeopen = 1;
    pi->nwrite = 0;
    pi->nread = 0;
    initlock(&pi->lock, "pipe");
    (*f0)->type = FD_PIPE;
    (*f0)->readable = 1;
    (*f0)->writable = 0;
    (*f0)->pipe = pi;
    (*f0)->flags = O_RDONLY | (flags & PIPE2_FLAGS);
    (*f0)->off = 0;
    (*f1)->type = FD_PIPE;
    (*f1)->readable = 0;
    (*f1)->writable = 1;
    (*f1)->pipe = pi;
    (*f1)->flags = O_WRONLY | O_APPEND | (flags & PIPE2_FLAGS);
    (*f1)->off = 0;
    return 0;

bad:
    if(pi)
        kfree((char*)pi);
    if(*f0)
        fileclose(*f0);
    if(*f1)
        fileclose(*f1);
    return -1;
}

void
pipeclose(struct pipe *pi, int writable)
{
    acquire(&pi->lock);
    if (writable) {
        pi->writeopen = 0;
        wakeup(&pi->nread);
    } else {
        pi->readopen = 0;
        wakeup(&pi->nwrite);
    }
    if (pi->readopen == 0 && pi->writeopen == 0) {
        release(&pi->lock);
        kfree((char*)pi);
    } else
        release(&pi->lock);
}

int
pipewrite(struct pipe *pi, uint64_t addr, int n)
{
    int i = 0;
    struct proc *pr = myproc();

    acquire(&pi->lock);
    while (i < n) {
        if (pi->readopen == 0 || killed(pr)) {
            release(&pi->lock);
            return -1;
        }
        if (pi->nwrite == pi->nread + PIPESIZE) { //DOC: pipewrite-full
            wakeup(&pi->nread);
            sleep(&pi->nwrite, &pi->lock);
        } else {
            char ch;
            if (copyin(pr->pagetable, &ch, addr + i, 1) == -1)
                break;
            pi->data[pi->nwrite++ % PIPESIZE] = ch;
            i++;
        }
    }
    wakeup(&pi->nread);
    release(&pi->lock);

    return i;
}

int
piperead(struct pipe *pi, uint64_t addr, int n)
{
    int i;
    struct proc *pr = myproc();
    char ch;

    acquire(&pi->lock);
    while (pi->nread == pi->nwrite && pi->writeopen) {  //DOC: pipe-empty
        if (killed(pr)) {
            release(&pi->lock);
            return -1;
        }
        sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
    }

    for (i = 0; i < n; i++) {  //DOC: piperead-copy
        if (pi->nread == pi->nwrite)
            break;
        ch = pi->data[pi->nread++ % PIPESIZE];
        if (copyout(pr->pagetable, addr + i, &ch, 1) == -1)
            break;
    }
    wakeup(&pi->nwrite);  //DOC: piperead-wakeup
    release(&pi->lock);
    return i;
}
