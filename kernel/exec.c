#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include <common/file.h>
#include <spinlock.h>
#include <proc.h>
#include <defs.h>
#include <elf.h>
#include <printf.h>

static int loadseg(pde_t *, uint64_t, struct inode *, uint32_t, uint32_t);

int flags2perm(int flags)
{
    int perm = 0;
    if (flags & 0x1)
      perm = PTE_X;
    if (flags & 0x2)
      perm |= PTE_W;
    return perm;
}

int
exec(char *path, char **argv)
{
    char *s, *last;
    int i, off;
    uint64_t argc, sz = 0, sp, ustack[MAXARG], stackbase;
    struct elfhdr elf;
    struct inode *ip;
    struct proghdr ph;
    pagetable_t pagetable = 0, oldpagetable;
    struct proc *p = myproc();

    begin_op();

    if ((ip = namei(path)) == 0) {
        end_op();
        debug("path: %s couldn't find", path);
        return -1;
    }
    ilock(ip);

    // Check ELF header
    if (readi(ip, 0, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        debug("readi error: inum=%d", ip->inum);
        goto bad;
    }

    if (elf.magic != ELF_MAGIC) {
        debug("bad magic: 0x%08x", elf.magic);
        goto bad;
    }


    if ((pagetable = proc_pagetable(p)) == 0) {
        debug("no pagetable: pid=%d", p->pid);
        goto bad;
    }


    // Load program into memory.
    for (i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)) {
        if (readi(ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph)) {
            debug("read prog header[%d], off: 0x%08x", i, off);
            goto bad;
        }

        if (ph.type != ELF_PROG_LOAD)
            continue;
        if (ph.memsz < ph.filesz) {
            debug("size error: memsz 0x%16x < filesz 0x%16x", ph.memsz, ph.filesz);
            goto bad;
        }

        if (ph.vaddr + ph.memsz < ph.vaddr) {
            debug("addr error: vaddr 0x%16x + memsz 0x%16x < vaddr 0x%16x", ph.vaddr, ph.memsz, ph.vaddr);
            goto bad;
        }

        if (ph.vaddr % PGSIZE != 0) {
            debug("vaddr not align: 0x%16x", ph.vaddr);
            goto bad;
        }

        uint64_t sz1;
        if ((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0) {
            debug("uvmalloc error: sz1 = 0x%16x", sz1);
            goto bad;
        }
        sz = sz1;
        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
            debug("loadseg error: inum: %d, off: 0x%16x", ip->inum, ph.off);
            goto bad;
        }

    }
    iunlockput(ip);
    end_op();
    ip = 0;

    // Synchronize the instruction and data streams.
    asm volatile("fence.i");

    p = myproc();
    uint64_t oldsz = p->sz;

    // Allocate two pages at the next page boundary.
    // Make the first inaccessible as a stack guard.
    // Use the second as the user stack.
    sz = PGROUNDUP(sz);
    uint64_t sz1;
    if ((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE, PTE_W)) == 0) {
        debug("uvmalloc for page coundary error");
        goto bad;
    }

    sz = sz1;
    uvmclear(pagetable, sz-2*PGSIZE);
    sp = sz;
    stackbase = sp - PGSIZE;

    // Push argument strings, prepare rest of stack in ustack.
    for (argc = 0; argv[argc]; argc++) {
        if(argc >= MAXARG) {
            debug("too big argc: %d", argc);
            goto bad;
        }
        sp -= strlen(argv[argc]) + 1;
        sp -= sp % 16; // riscv sp must be 16-byte aligned
        if(sp < stackbase) {
            debug("sp: 0x%16x exeed stackbase: 0x%16x", sp, stackbase);
            goto bad;
        }
        if (copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
            debug("copyout error: argv[%d]", argc);
            goto bad;
        }

        ustack[argc] = sp;
    }
    ustack[argc] = 0;

    // push the array of argv[] pointers.
    sp -= (argc+1) * sizeof(uint64_t);
    sp -= sp % 16;
    if (sp < stackbase) {
        debug("[2] sp: 0x%16x exeed stackbase: 0x%16x", sp, stackbase);
        goto bad;
    }

    if (copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64_t)) < 0) {
        debug("copyout error: ustack: 0x%p", (char *)ustack);
        goto bad;
    }


    // arguments to user main(argc, argv)
    // argc is returned via the system call return
    // value, which goes in a0.
    p->trapframe->a1 = sp;

    // Save program name for debugging.
    for(last=s=path; *s; s++)
        if (*s == '/')
            last = s+1;
    safestrcpy(p->name, last, sizeof(p->name));

    // Commit to the user image.
    oldpagetable = p->pagetable;
    p->pagetable = pagetable;
    p->sz = sz;
    p->trapframe->epc = elf.entry;  // initial program counter = main
    p->trapframe->sp = sp; // initial stack pointer
    proc_freepagetable(oldpagetable, oldsz);

    return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
    if (pagetable)
        proc_freepagetable(pagetable, sz);
    if (ip) {
        iunlockput(ip);
        end_op();
    }
    return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64_t va, struct inode *ip, uint32_t offset, uint32_t sz)
{
  uint32_t i, n;
  uint64_t pa;

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64_t)pa, offset+i, n) != n)
      return -1;
  }

  return 0;
}
