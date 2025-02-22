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
#include <linux/auxvec.h>

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
execve(char *path, char *const argv[], char *const envp[], int argc, int envc)
{
    char *s, *last;
    int i, off;
    uint64_t sz = 0, sp, ustack[MAXARG], estack[MAXARG], stackbase;
    struct elfhdr elf;
    struct inode *ip;
    struct proghdr ph;
    pagetable_t pagetable = 0, oldpagetable;
    struct proc *p = myproc();
    uint64_t sp_top;

    begin_op();

    if ((ip = namei(path)) == 0) {
        end_op();
        warn("path: %s couldn't find", path);
        return -1;
    }
    ilock(ip);

    trace("path: %s, ip: %d", path, ip->inum);
    // Check ELF header
    if (readi(ip, 0, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        warn("readi elf error: inum=%d", ip->inum);
        goto bad;
    }

    if (elf.magic != ELF_MAGIC) {
        warn("bad magic: 0x%08x", elf.magic);
        goto bad;
    }

    // trampoline/p->trapframe/tlsをマッピングしたユーザページテーブルを作成する
    if ((pagetable = proc_pagetable(p)) == 0) {
        warn("no pagetable: pid=%d", p->pid);
        goto bad;
    }

    int nph = 0;

    // プログラムをメモリにロードする.
    for (i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)) {
        if (readi(ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph)) {
            warn("read prog header[%d], off: 0x%08x", i, off);
            goto bad;
        }

        if (ph.type != ELF_PROG_LOAD)
            continue;
        if (ph.memsz < ph.filesz) {
            warn("size error: memsz 0x%lx < filesz 0x%lx", ph.memsz, ph.filesz);
            goto bad;
        }

        if (ph.vaddr + ph.memsz < ph.vaddr) {
            warn("addr error: vaddr 0x%lx + memsz 0x%lx < vaddr 0x%lx", ph.vaddr, ph.memsz, ph.vaddr);
            goto bad;
        }

        // 最初のプログラムヘッダーはPGSIZEにアラインされていなければならない
        if (nph == 0) {
            sz = ph.vaddr;
            if (sz % PGSIZE != 0) {
                warn("first section should be page aligned: 0x%lx", sz);
                goto bad;
            }
        }
        nph++;

        // コード用のメモリを割り当ててマッピング
        uint64_t sz1;
        if ((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0) {
            warn("uvmalloc error: sz1 = 0x%lx", sz1);
            goto bad;
        }
        trace("LOAD[%d] sz: 0x%lx, sz1: 0x%lx, flags: 0x%08x", i, sz, sz1, flags2perm(ph.flags));
        trace("         addr: 0x%lx, off: 0x%lx, fsz: 0x%lx", ph.vaddr, ph.off, ph.filesz);

        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
            warn("loadseg error: inum: %d, off: 0x%lx", ip->inum, ph.off);
            goto bad;
        }
#if 0
        pte_t *pte = walk(pagetable, ph.vaddr, 0);
        debub("va: 0x%lx, pte: %p, *pte: 0x%lx", ph.vaddr, pte, *pte);
        if (nph == 2) {
            char var[8];
            pte_t *pte = walk(pagetable, 0x18648, 0);
            debug("memsz: 0x18648, pte: %p, *pte: 0x%lx", pte, *pte);
            copyin(pagetable, var, 0x18648, 8);
            debug("*0x18648: 0x%02x %02x %02x %02x %02x %02x %02x %02x",
                var[0], var[1], var[2], var[3], var[4], var[5], var[6], var[7]);
        }
#endif
        sz = sz1;
        fence_i();
    }
    iunlockput(ip);
    end_op();
    ip = 0;

    // Synchronize the instruction and data streams.
    fence_i();

    //p = myproc();
    uint64_t oldsz = p->sz;

    // 次のページ境界に3ページ割り当てる。
    // 最初のページをスタックガードとしてアクセス不能にする。
    // 2-3番目をユーザスタックとして使用する。
    sz = PGROUNDUP(sz);
    uint64_t sz1;
    if ((sz1 = uvmalloc(pagetable, sz, sz + 3*PGSIZE, PTE_W)) == 0) {
        warn("uvmalloc for page boundary error");
        goto bad;
    }

    sp = sp_top = sz = sz1;
    // 1ページ目をガードページとしてユーザアクセス禁止とする
    uvmclear(pagetable, sz - 3*PGSIZE);
    stackbase = sp - 2 * PGSIZE;
    trace("sp: 0x%lx, stackbase: 0x%lx", sp, stackbase);

    // uint64_t sp_top = sp;
    // 引数文字列をプッシュし、残りのスタックをustackに準備する
    if (argc > 0) {
        for (i = 0; i < argc; i++) {
            sp -= strlen(argv[i]) + 1;
            sp -= sp % 16; // riscv sp must be 16-byte aligned
            if (sp < stackbase) {
                warn("sp: 0x%lx exeed stackbase for arg[%d]: 0x%lx", sp, i, stackbase);
                goto bad;
            }
            if (copyout(pagetable, sp, argv[i], strlen(argv[i]) + 1) < 0) {
                warn("copyout error: argv[%d]", i);
                goto bad;
            }
            ustack[i] = sp;
            if (p->pid == 3)
                trace("ustack[%d]: 0x%016lx, argv[%d]: %s", i, ustack[i], i, argv[i]);
        }
    }
    ustack[i] = 0;
    if (p->pid == 3)
        trace("ustack[%d]: 0x%016lx", i, ustack[i]);

    if (envc > 0) {
        // 引数文字列をプッシュし、残りのスタックをustackに準備する
        for (i = 0; i < envc; i++) {
            sp -= strlen(envp[i]) + 1;
            sp -= sp % 16; // riscv sp must be 16-byte aligned
            if (sp < stackbase) {
                warn("sp: 0x%lx exeed stackbase for envp[%d]: 0x%lx", sp, i, stackbase);
                goto bad;
            }
            if (copyout(pagetable, sp, envp[i], strlen(envp[i]) + 1) < 0) {
                warn("copyout error: envp[%d]", i);
                goto bad;
            }
            estack[i] = sp;
            if (p->pid == 3)
                trace("estack[%d]: 0x%016lx, envp[%d]: %s", i, estack[i], i, envp[i]);
        }
        estack[i] = 0;
    }
    estack[i] = 0;
    if (p->pid == 3)
        trace("estack[%d]: 0x%016lx", i, estack[i]);

    uint64_t auxv_sta[][2] = { { AT_PAGESZ, PGSIZE }, { AT_NULL,    0 } };
    uint64_t auxv_size = sizeof(auxv_sta);
    sp -= auxv_size;
    sp -= sp % 16;
    trace("auxv sp: %p, size: %ld", sp, auxv_size);
    if (sp < stackbase) {
        warn("[2] sp: 0x%lx exeed stackbase for aux: 0x%lx", sp, stackbase);
        goto bad;
    }
    if (copyout(pagetable, sp, (char *)auxv_sta, auxv_size) < 0) {
        warn("copyout error: auxv");
        goto bad;
    }

    // push the array of argv[] pointers.
    uint64_t sp_envp = 0;
    if (envc > 0) {
        sp -= (envc + 1) * sizeof(uint64_t);
        sp -= sp % 16;
        if (sp < stackbase) {
            warn("[2] sp: 0x%lx exeed stackbase for env: 0x%lx", sp, stackbase);
            goto bad;
        }
        if (copyout(pagetable, sp, (char *)estack, (envc+1)*sizeof(uint64_t)) < 0) {
            warn("copyout error: estack: %p", (char *)estack);
            goto bad;
        }
        sp_envp = sp;
    }

    // push the array of argv[] pointers.
    sp -= (argc + 1) * sizeof(uint64_t);
    sp -= sp % 16;
    if (sp < stackbase) {
        warn("[2] sp: 0x%lx exeed stackbase for arg: 0x%lx", sp, stackbase);
        goto bad;
    }
    if (copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64_t)) < 0) {
        warn("copyout error: ustack: %p", (char *)ustack);
        goto bad;
    }
    trace("path: %s, argc: %d, envc: %d", path, argc, envc);
    // ユーザのmain(argc, argv, envp)への引数
    // argcはシステムコールの戻り値としてa0に入れられて返される。
    p->trapframe->a1 = sp;
    p->trapframe->a2 = sp_envp;

    // デバッグ用にプログラム名を保存する .
    for (last=s=path; *s; s++)
        if (*s == '/')
            last = s+1;
    safestrcpy(p->name, last, sizeof(p->name));

    // Commit to the user image.
    oldpagetable = p->pagetable;
    p->pagetable = pagetable;
    p->sz = sz;
    p->trapframe->epc = elf.entry;  // initial program counter = main
    p->trapframe->sp = sp; // initial stack pointer
    p->trapframe->tp = TRAPFRAME;

    proc_freepagetable(oldpagetable, oldsz);

#if 0
    uvmdump(p->pagetable, p->pid, p->name);
    printf("\n== Stack TOP : 0x%l08x ==\n", sp_top);
    for (uint64_t e = sp_top - 8; e >= sp; e -= 8) {
        uint64_t val;
        copyin(pagetable, (char *)&val, e, 8);
        printf("%l08x: %l016x\n", e, val);
    }
    printf("== Stack END : ox%l08x ==\n\n", sp);
#endif
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

// プログラムセグメントを仮想アドレスvaのpagetableにロードする。
// vaはページにアラインされており、vaからva+szまでのページが
// マップ済みである必要がある。
// 成功の場合は0を、失敗の場合は-1を返す。
static int
loadseg(pagetable_t pagetable, uint64_t va, struct inode *ip, uint32_t offset, uint32_t sz)
{
    uint32_t i, n;
    uint64_t pa;

    for (i = 0; i < sz; i += PGSIZE) {
        pa = walkaddr(pagetable, va + i);
        if (pa == 0)
            panic("loadseg: address should exist");
        if (sz - i < PGSIZE)
            n = sz - i;
        else
            n = PGSIZE;
        if (readi(ip, 0, (uint64_t)pa, offset+i, n) != n)
            return -1;
    }

    return 0;
}
