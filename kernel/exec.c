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

    // trampolineとp->trapframeだけをマッピングしたユーザページテーブルを作成する
    if ((pagetable = proc_pagetable(p)) == 0) {
        warn("no pagetable: pid=%d", p->pid);
        goto bad;
    }

    int first = 1;

    // プログラムをメモリにロードする.
    for (i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)) {
        if (readi(ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph)) {
            warn("read prog header[%d], off: 0x%08x", i, off);
            goto bad;
        }

        if (ph.type != ELF_PROG_LOAD)
            continue;
        if (ph.memsz < ph.filesz) {
            warn("size error: memsz 0x%l016x < filesz 0x%l016x", ph.memsz, ph.filesz);
            goto bad;
        }

        if (ph.vaddr + ph.memsz < ph.vaddr) {
            warn("addr error: vaddr 0x%l016x + memsz 0x%l016x < vaddr 0x%l016x", ph.vaddr, ph.memsz, ph.vaddr);
            goto bad;
        }

        // 最初のプログラムヘッダーはPGSIZEにアラインされていなければならない
        if (first) {
            first = 0;
            if (ph.vaddr % PGSIZE != 0) {
                warn("vaddr not align: 0x%l016x", ph.vaddr);
                goto bad;
            }
        }

        // コード用のメモリを割り当ててマッピング
        uint64_t sz1;
        if ((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0) {
            warn("uvmalloc error: sz1 = 0x%l016x", sz1);
            goto bad;
        }
        trace("LOAD[%d] sz: %ld, sz1: %ld, flags: 0x%08x", i, sz, sz1, ph.flags);
        sz = sz1;
        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
            warn("loadseg error: inum: %d, off: 0x%l016x", ip->inum, ph.off);
            goto bad;
        }
        asm volatile("fence.i");
    }
    iunlockput(ip);
    end_op();
    ip = 0;

    // Synchronize the instruction and data streams.
    asm volatile("fence.i");

    p = myproc();
    uint64_t oldsz = p->sz;

    // 次のページ境界に2ページ割り当てる。
    // 最初のページをスタックガードとしてアクセス不能にする。
    // 2番目をユーザスタックとして使用する。
    sz = PGROUNDUP(sz);
    uint64_t sz1;
    if ((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE, PTE_W)) == 0) {
        warn("uvmalloc for page boundary error");
        goto bad;
    }

    sz = sz1;
    // 1ページ目をガードページとしてユーザアクセス禁止とする
    uvmclear(pagetable, sz - 2*PGSIZE);
    sp = sz;
    stackbase = sp - PGSIZE;
    trace("sp: 0x%lx, base: 0x%lx", sp, stackbase);

    // tls用に1ページ確保する
    if ((sz1 = uvmalloc(pagetable, sz, sz + PGSIZE, PTE_W)) == 0) {
        warn("uvmalloc for tls page error");
        goto bad;
    }

    // 引数文字列をプッシュし、残りのスタックをustackに準備する
    for (argc = 0; argv[argc]; argc++) {
        if (argc >= MAXARG) {
            warn("too big argc: %d", argc);
            goto bad;
        }
        sp -= strlen(argv[argc]) + 1;
        sp -= sp % 16; // riscv sp must be 16-byte aligned
        if (sp < stackbase) {
            warn("sp: 0x%l016x exeed stackbase for arg: 0x%l016x", sp, stackbase);
            goto bad;
        }
        if (copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
            warn("copyout error: argv[%d]", argc);
            goto bad;
        }

        ustack[argc] = sp;
    }
    ustack[argc] = 0;

    // push the array of argv[] pointers.
    sp -= (argc + 1) * sizeof(uint64_t);
    sp -= sp % 16;
    if (sp < stackbase) {
        warn("[2] sp: 0x%l016x exeed stackbase for arg: 0x%l016x", sp, stackbase);
        goto bad;
    }

    if (copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64_t)) < 0) {
        warn("copyout error: ustack: 0x%p", (char *)ustack);
        goto bad;
    }

    // ユーザのmain(argc, argv)への引数
    // argcはシステムコールの戻り値としてa0に入れられて返される。
    p->trapframe->a1 = sp;

    // デバッグ用にプログラム名を保存する .
    for (last=s=path; *s; s++)
        if (*s == '/')
            last = s+1;
    safestrcpy(p->name, last, sizeof(p->name));

    // Commit to the user image.
    oldpagetable = p->pagetable;
    p->pagetable = pagetable;
    p->sz = sz1;
    p->trapframe->epc = elf.entry;  // initial program counter = main
    p->trapframe->sp = sp; // initial stack pointer
    p->trapframe->tp = sz1;
    trace("tp: 0x%l016x", sz1);
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

// プログラムセグメントを仮想アドレスvaのpagetableにロードする。
// vaはページにアラインされており、vaからva+szまでのページが
// マップ済みである必要がある。
// 成功の場合は0を、失敗の場合は-1を返す。
static int
loadseg(pagetable_t pagetable, uint64_t va, struct inode *ip, uint32_t offset, uint32_t sz)
{
    uint32_t i, n;
    uint64_t pa;

    for (i = 0; i < sz; i += PGSIZE){
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
