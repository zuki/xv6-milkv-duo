#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include <exec.h>
#include <common/file.h>
#include <spinlock.h>
#include <proc.h>
#include <defs.h>
#include <elf.h>
#include <printf.h>
#include <errno.h>
#include <linux/mman.h>
#include <linux/auxvec.h>
#include <linux/stat.h>
#include <linux/capability.h>

static int loadseg(pde_t *, uint64_t, struct inode *, uint32_t, uint32_t);

static void flush_parent_data(struct proc *p)
{
    // (1) signalを開放
    flush_signal_handlers(p);
    // (2) close_on_execのfileをclose
    for (int i = 0; i < NOFILE; i++) {
        if (p->ofile[i] && bit_test(p->fdflag, i)) {
            fileclose(p->ofile[i]);
            p->ofile[i] = 0;
            bit_remove(p->fdflag, i);
        }
    }
    // (3) capabilityを再設定
    cap_clear(p->cap_inheritable);
    cap_clear(p->cap_permitted);
    cap_clear(p->cap_effective);

    if (p->uid == 0 || p->euid == 0) {
        cap_set_full(p->cap_inheritable);
        cap_set_full(p->cap_permitted);
    }

    if (p->euid == 0 || p->fsuid == 0)
        cap_set_full(p->cap_effective);

    // (4) mmap領域を解放
    free_mmap_list(p);
}

static int flags2perm(int flags)
{
    int perm = 0;
    if (flags & PF_X)
      perm = PTE_X;
    if (flags & PF_W)
      perm |= PTE_W;
    if (flags & PF_R)
      perm |= PTE_R;
    return perm;
}

// elf_bssからページ境界までゼロクリアする
// |----elf_bss_0000000000000000|
static void padzero(uint64_t elf_bss)
{
    uint64_t nbyte;
    char *buf;

    nbyte = ELF_PAGEOFFSET(elf_bss);
    trace("elf_bss=0x%llx, nbyte=0x%llx", elf_bss, nbyte ? (ELF_MIN_ALIGN - nbyte) : 0);
    if (nbyte) {
        nbyte = ELF_MIN_ALIGN - nbyte;
        buf = kmalloc(nbyte);
        memset(buf, 0, nbyte);
        copyout(myproc()->pagetable, elf_bss, buf, nbyte);
        kmfree(buf);
    }
}

static char *mapping_bss(uint64_t start, uint64_t end)
{
    char  *old_start = (char *)start;
    //char  *old_end   = (char *)end;

    trace("start: 0x%lx, end: 0x%lx", start, end);
    start = ELF_PAGEALIGN(start);   // roundup
    end = ELF_PAGEALIGN(end);       // roundup
    // 既存のマッピング内に収まれば何もしない（0詰め済み）
    if (end <= start) {
        padzero((uint64_t)old_start);
        return old_start;
    }

    trace("mmap start: 0x%lx (%p), end: 0x%lx (%p), length: 0x%lx", start, old_start, end, old_end, end - start);
    // mappingを行う
    return (char *)mmap((void *)start, end - start, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, NULL, 0);
}

static uint64_t load_interpreter(char *path, uint64_t *base)
{
    struct file *f;
    int i, off, addr_fix = 0;
    struct elfhdr elf;
    struct proghdr ph;
    uint64_t load_addr = 0, bss_start = 0, bss_end = 0;
    char *mapped;
    long errno = 0;

    trace("interp: %s", path);

    // 1. インタプリタファイルを取得
    f = fileget(path);
    if (IS_ERR(f))
        return (uint64_t)f;

    // 2. ELFヘッダーのチェック
    if (readi(f->ip, 0, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        warn("readi elf error: inum=%d", f->ip->inum);
        errno = -EIO;
        goto out;
    }
    errno = -ENOEXEC;
    if (elf.type != ET_DYN && elf.type != ET_EXEC)
        goto out;
    if (!ELF_CHECK_ARCH(&elf))
        goto out;
    if (elf.phentsize != sizeof(struct proghdr))
        goto out;
    if (elf.phnum > ELF_MIN_ALIGN / sizeof(struct proghdr))
        goto out;

    // 3. プログラムヘッダを処理
    for (i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)) {
        int flags = 0;
        int prot  = 0;
        uint64_t vaddr = 0;
        uint64_t k;

        // 3.1 ヘッダを読み込む
        if (readi(f->ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph)) {
            warn("read prog header[%d], off: 0x%08x", i, off);
            errno = -EIO;
            goto out;
        }

        // PT_LOAD以外は処理不要
        if (ph.type != PT_LOAD)
            continue;

        // 3.2 ヘッダの妥当性をチェック
        errno = -ENOEXEC;
        if (ph.memsz < ph.filesz) {
            warn("size error: memsz 0x%lx < filesz 0x%lx", ph.memsz, ph.filesz);
            goto out;
        }

        if (ph.vaddr + ph.memsz < ph.vaddr) {
            warn("addr error: vaddr 0x%lx + memsz 0x%lx < vaddr 0x%lx", ph.vaddr, ph.memsz, ph.vaddr);
            goto out;
        }

        // 3.3 mmap用のprot変数をセット
        if (ph.flags & PF_R) prot |= PROT_READ;
        if (ph.flags & PF_W) prot |= PROT_WRITE;
        if (ph.flags & PF_X) prot |= PROT_EXEC;

        // 3.3 mmap用のflags変数をセット
        //   テキスト領域はMAP_SHARED、データ領域はMAP_PRIVATE
        if (prot & PROT_WRITE)
            flags = MAP_PRIVATE;
        else
            flags = MAP_SHARED;

        vaddr = ph.vaddr;

        // ET_EXECまたは2番目以降のプログラムヘッダはアドレス固定
        if (elf.type == ET_EXEC || addr_fix)
            flags |= MAP_FIXED;
        // それ以外は固定ではないがload_addrの初期値を与える
        else
            load_addr = ELF_PAGESTART(ELF_ET_DYN_BASE + vaddr);

        // 3.4 データをmappingする
        trace("start: 0x%lx  = load_addr: 0x%lx + vaddr: 0x%lx, ", ELF_PAGESTART(load_addr + vaddr), load_addr, vaddr);
        mapped = (char *)mmap(
            (void *)ELF_PAGESTART(load_addr + vaddr),
            ph.filesz  + ELF_PAGEOFFSET(vaddr),
            prot, flags, f,
            ph.off - ELF_PAGEOFFSET(vaddr));
        if (IS_ERR(mapped)) {
            errno = (long)mapped;
            goto out;
        }
        trace("mapped: 0x%lx", mapped);

        // 3.5 ロードアドレスを調整する
        if (addr_fix == 0 && elf.type == ET_DYN) {
            load_addr = (uint64_t)mapped - ELF_PAGESTART(vaddr);
            addr_fix = 1;
        }

        // 3.6 bss領域をセットする
        k = load_addr + vaddr + ph.filesz;
        if (k > bss_start) bss_start = k;
        k = load_addr + vaddr + ph.memsz;
        if (k > bss_end) bss_end = k;
        trace("bss_start: 0x%lx, bss_end: %lx", bss_start, bss_end);
    }

    // 4. 必要があればbss領域を0クリアしてmappingする
    padzero(bss_start);
    bss_start = ELF_PAGESTART(bss_start + ELF_MIN_ALIGN - 1);
    if (bss_end > bss_start) {
        mapped = mapping_bss(bss_start, bss_end);
        if (IS_ERR(mapped)) {
            errno = (long)mapped;
            goto out;
        }
    }
    *base = load_addr;
    errno = ((uint64_t)elf.entry) + load_addr;

out:
    fileclose(f);
    return (uint64_t)errno;
}

int execve(char *path, char *const argv[], char *const envp[], int argc, int envc)
{
    char *s, *last, *interp = 0;
    int has_interp = 0;             // インタプリタを持つか(dynamicか)
    uint64_t interp_entry = 0;      // インタプリタのエントリポイント
    uint64_t interp_base = 0;       // インタプリタのベースアドレス
    int i, off, errno = 0;
    uint64_t sp = USERTOP, len, ustack[MAXARG], estack[MAXARG];
    struct inode *ip;
    struct elfhdr elf;
    struct proghdr ph;
    pagetable_t pagetable = 0, oldpagetable;
    struct proc *p = myproc();

    begin_op();

    if ((ip = namei(path, AT_FDCWD)) == 0) {
        end_op();
        warn("path: %s couldn't find", path);
        return -ENOENT;
    }
    ilock(ip);

    trace("path: %s, ip: %d", path, ip->inum);
    // Check ELF header
    if (readi(ip, 0, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        warn("readi elf error: inum=%d", ip->inum);
        errno = -EIO;
        goto bad;
    }

    if (elf.magic != ELF_MAGIC) {
        warn("bad magic: 0x%08x", elf.magic);
        errno = -ENOEXEC;
        goto bad;
    }

    // trampoline/p->trapframeをマッピングしたユーザページテーブルを作成する
    if ((pagetable = proc_pagetable(p)) == 0) {
        warn("couldn't make pagetable: pid=%d", p->pid);
        errno = -ENOMEM;
        goto bad;
    }

    // ファイルのSet-uidをセットする: stickyビットの対応
    if (ip->mode & S_ISUID && p->uid != 0)
        p->fsuid = ip->uid;
    else
        p->fsuid = p->uid;
    // ファイルのSet-gidをセットする: stickyビットの対応
    if (((ip->mode & (S_ISGID | S_IXGRP)) == (S_ISGID | S_IXGRP)) && p->gid != 0)
        p->fsgid = ip->gid;
    else
        p->fsgid = p->gid;

    // 親プロセスから受け継いだ情報を破棄する
    flush_parent_data(p);

    // pagetableを設定する
    oldpagetable = p->pagetable;
    p->pagetable = pagetable;

    int nph = 0;
    uint64_t sz = 0;
    // プログラムをメモリにロードする.
    for (i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)) {
        if (readi(ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph)) {
            warn("read prog header[%d], off: 0x%08x", i, off);
            errno = -EIO;
            goto bad;
        }
        // PT_INTERPを処理: interpreterの文字列取得
        if (ph.type == PT_INTERP) {
            if (has_interp) {
                warn("there are multi interpretors");
                errno = -EINVAL;
                goto bad;
            }
            if (ph.filesz > PGSIZE) {      // インタプリタ名が長すぎる
                warn("interpreter name is too long: 0x%lx", ph.filesz);
                errno = -ENAMETOOLONG;
                goto bad;
            }
            interp = (char *)kmalloc(ph.filesz);
            if (!interp) {
                warn("no memory for interpreter name");
                errno = -ENOMEM;
                goto bad;
            }
            // fileszにはNULLを含む
            int bytes;
            if ((bytes = readi(ip, 0, (uint64_t)interp, ph.off, ph.filesz)) != ph.filesz) {
                warn("read interpreter name: off: 0x%08x, fsize: 0x%lx, bytes: 0x%x", ph.off, ph.filesz, bytes);
                errno = -EIO;
                goto bad;
            }
            has_interp = 1;
            continue;
        }
        // PT_LOAD以外は処理不要
        if (ph.type != PT_LOAD)
            continue;

        errno = -ENOEXEC;
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
            errno = -ENOMEM;
            goto bad;
        }
        trace("LOAD[%d] sz: 0x%lx, sz1: 0x%lx, flags: 0x%08x", i, sz, sz1, flags2perm(ph.flags));
        trace("         addr: 0x%lx, off: 0x%lx, fsz: 0x%lx", ph.vaddr, ph.off, ph.filesz);

        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
            warn("loadseg error: inum: %d, off: 0x%lx", ip->inum, ph.off);
            errno = -EIO;
            goto bad;
        }
        sz = sz1;
        fence_i();
    }
    iunlockput(ip);
    end_op();
    ip = 0;

    // Synchronize the instruction and data streams.
    fence_i();

    // インタプリタをロードする
    if (has_interp) {
        interp_entry = load_interpreter(interp, &interp_base);
        if (IS_ERR((void *)interp_entry)) {
            error("interpreter load error: %ld", interp_entry);
            goto bad;
        }
        trace("interp_base: 0x%lx, interp_entry: 0x%lx", interp_base, interp_entry);
    }

    // スタックを割り当てる; STACKBASEからSTACKTOPまでのページを割り当てる
    if ((sp = uvmalloc(pagetable, STACKBASE, STACKTOP, PTE_W)) == 0) {
        warn("uvmalloc for stack is failed");
        errno = -ENOMEM;
        goto bad;
    }

    uint64_t platform = 0;
    if (has_interp) {
        len = strlen(ELF_PLATFORM) + 1;
        sp -= len;
        sp -= sp % 16;
        platform = (uint64_t)sp;
        if (copyout(pagetable, sp, ELF_PLATFORM, len) < 0) {
            warn("copyout error: platform");
            goto bad;
        }
    }

    // 引数文字列をプッシュし、残りのスタックをustackに準備する
    for (i = 0; i < argc; i++) {
        sp -= strlen(argv[i]) + 1;
        sp -= sp % 16; // riscv sp must be 16-byte aligned
        if (copyout(pagetable, sp, argv[i], strlen(argv[i]) + 1) < 0) {
            warn("copyout error: argv[%d]", i);
            errno = -EIO;
            goto bad;
        }
        ustack[i] = sp;
        if (p->pid == 3)
            trace("ustack[%d]: 0x%016lx, argv[%d]: %s", i, ustack[i], i, argv[i]);
    }
    ustack[i] = 0;
    if (p->pid == 3)
        trace("ustack[%d]: 0x%016lx", i, ustack[i]);


    // 引数文字列をプッシュし、残りのスタックをustackに準備する
    for (i = 0; i < envc; i++) {
        sp -= strlen(envp[i]) + 1;
        sp -= sp % 16; // riscv sp must be 16-byte aligned
        if (copyout(pagetable, sp, envp[i], strlen(envp[i]) + 1) < 0) {
            warn("copyout error: envp[%d]", i);
            errno = -EIO;
            goto bad;
        }
        estack[i] = sp;
        if (p->pid == 3)
            trace("estack[%d]: 0x%016lx, envp[%d]: %s", i, estack[i], i, envp[i]);
    }
    estack[i] = 0;
    if (p->pid == 3)
        trace("estack[%d]: 0x%016lx", i, estack[i]);

    uint64_t auxv_size;
    uint64_t auxv_dyn[][2] = {
        { AT_PHDR,    elf.phoff },
        { AT_PHENT,   sizeof(struct proghdr) },
        { AT_PHNUM,   elf.phnum },
        { AT_PAGESZ,  PGSIZE },
        { AT_BASE,    interp_base },            // インタプリタのbaseアドレス
        { AT_FLAGS,   0 },
        { AT_ENTRY,   elf.entry },              // プログラムのエントリポイント
        { AT_UID,     (uint64_t) p->uid },
        { AT_EUID,    (uint64_t) p->euid },
        { AT_GID,     (uint64_t) p->gid },
        { AT_EGID,    (uint64_t) p->egid },
        { AT_PLATFORM, platform },
        { AT_HWCAP,   ELF_HWCAP },
        { AT_CLKTCK,  100 },
        { AT_NULL,    0 }
    };
    uint64_t auxv_sta[][2] = { { AT_PAGESZ, PGSIZE }, { AT_NULL,    0 } };

    if (has_interp) {
        auxv_size = sizeof(auxv_dyn);
    } else {
        auxv_size = sizeof(auxv_sta);
    }

    // auxv, estack, ustack, argc を詰めてセット
    // estack, ustackはNULL終端の配列
    uint64_t u64cnt = envc + 1 + argc + 1 + 1;
    sp -= u64cnt * sizeof(uint64_t) + auxv_size;
    sp -= sp % 16;

    // spは16バイトアラインされているので、argcから逆順につめる
    uint64_t u64_argc = argc;
    if (copyout(pagetable, sp, (char *)&u64_argc, sizeof(uint64_t)) < 0) {
        warn("copyout argc error: %p", &u64_argc);
        errno = -EIO;
        goto bad;
    }

    if (copyout(pagetable, sp + sizeof(uint64_t), (char *)ustack, (argc+1)*sizeof(uint64_t)) < 0) {
        warn("copyout ustack error: %p", (char *)ustack);
        errno = -EIO;
        goto bad;
    }

    if (copyout(pagetable, sp + ((argc+2) * sizeof(uint64_t)), (char *)estack, (envc+1)*sizeof(uint64_t)) < 0) {
        warn("copyout error: estack: %p", (char *)estack);
        errno = -EIO;
        goto bad;
    }

    if (copyout(pagetable, sp + ((envc + argc + 3) * sizeof(uint64_t)), has_interp ? (char *)auxv_dyn : (char *)auxv_sta, auxv_size) < 0) {
        warn("copyout auxv error: %p", has_interp ? auxv_dyn : auxv_sta);
        errno = -EIO;
        goto bad;
    }

    trace("path: %s, argc: %d, envc: %d", path, argc, envc);
    // void _start_c(long *p) {
    //      int argc = p[0];
    //      char **argv = (void *)(p+1);
    //p->trapframe->a1 = sp;
    //p->trapframe->a2 = sp_envp;

    // デバッグ用にプログラム名を保存する .
    for (last=s=path; *s; s++)
        if (*s == '/')
            last = s+1;
    safestrcpy(p->name, last, sizeof(p->name));

    uint64_t oldsz = p->sz;
    //oldpagetable = p->pagetable;
    //p->pagetable = pagetable;
    p->sz = sz;
    // 実行するプログラムカウンタは動的リンクの場合はインタプリタの、
    // 静的リンクの場合はプログラムのエントリポインタ
    if (has_interp)
        p->trapframe->epc = interp_entry;
    else
        p->trapframe->epc = elf.entry;
    // スタックポインタ
    p->trapframe->sp = sp;
    trace("pid[%d] sz: 0x%lx, sp: 0x%lx, interp: %d, epc: 0x%lx", p->pid, p->sz, p->trapframe->sp, has_interp, p->trapframe->epc);

#if 0
    debug("free old pagetable: pid=%d", p->pid);
    //if (p->pid == 8) {
        uvmdump(pagetable, p->pid, "new");
        uvmdump(oldpagetable, p->pid, "old");
    //}
    //print_mmap_list(p, "new proc");
#endif

    proc_freepagetable(oldpagetable, oldsz);

#if 0
    if (p->pid == 7) {
        uvmdump(p->pagetable, p->pid, p->name);
        printf("\n== Stack TOP : 0x%08lx ==\n", STACKTOP);
        for (uint64_t e = STACKTOP - 8; e >= sp; e -= 8) {
            uint64_t val;
            copyin(pagetable, (char *)&val, e, 8);
            printf("%08lx: %016lx\n", e, val);
        }
        printf("== Stack END : 0x%08lx ==\n\n", sp);
        print_mmap_list(p, "new proc");
    }
#endif
    return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
    if (interp)
        kmfree(interp);
    if (pagetable) {
        trace("free pagetable for bad");
        proc_freepagetable(pagetable, sz);
    }
    if (ip) {
        iunlockput(ip);
        end_op();
    }

    return errno;
}

// プログラムセグメントを仮想アドレスvaのpagetableにロードする。
// vaはページにアラインされており、vaからva+szまでのページは
// マップ済みである必要がある。
// 成功の場合は0を、失敗の場合は-1を返す。
static int
loadseg(pagetable_t pagetable, uint64_t va, struct inode *ip, uint32_t offset, uint32_t sz)
{
    uint32_t i, n;
    uint64_t pa;
    int bytes;

    // vaがページアラインしていない場合はページ内オフセットを考慮
    uint64_t addr = va & 0x0fffUL;           // 先頭ページ内のオフセット

    for (i = 0; i < sz; i += PGSIZE) {
        pa = walkaddr(pagetable, va);
        if (pa == 0)
            panic("loadseg: address should exist");
        if (addr > 0) {
            n = PGSIZE - addr > sz ? sz : PGSIZE - addr;
            pa += addr;                         // 先頭ページのコピー先アドレスを調整する
            sz = sz + PGSIZE - n;
            addr = 0;
        } else if (sz - i < PGSIZE) {
            n = sz - i;
        } else {
            n = PGSIZE;
        }
        trace("va: 0x%lx, pa: 0x%lx, off: 0x%x, n: 0x%x", va, pa, offset, n);
        if ((bytes = readi(ip, 0, pa, offset, n)) != n) {
            trace("n: 0x%x, bytes: 0x%x", n, bytes);
            return -1;
        }
        offset += n;
        va += n;
    }

    return 0;
}
