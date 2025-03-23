#include <common/param.h>
#include <common/types.h>
#include <common/memlayout.h>
#include <elf.h>
#include <common/riscv.h>
#include <defs.h>
#include <proc.h>
#include <common/fs.h>
#include <linux/mman.h>
#include <config.h>
#include <printf.h>
#ifdef DUO256
#include <cv181x_reg.h>
#else
#include <cv180x_reg.h>
#endif


/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
    pagetable_t kpgtbl;

    kpgtbl = (pagetable_t) kalloc();
    memset(kpgtbl, 0, PGSIZE);

    // 各種IOレジスタをマップする
    kvmmap(kpgtbl, UART0, UART0_PHY, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, MMIO_BASE, MMIO_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, CLKGEN_BASE, CLKGEN_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, PINMUX_BASE, PINMUX_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, RESET_BASE, RESET_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, RTC_CTRL_BASE, RTC_CTRL_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, RTC_CORE_BASE, RTC_CORE_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_DEVICE);
    kvmmap(kpgtbl, SD0_BASE, SD0_BASE, 16*PGSIZE, PTE_DEVICE);

    // カーネルのtext, read-onlyセクションをマップする.
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64_t)etext-KERNBASE, PTE_EXEC);

    // カーネルのデータセクションから最大物理RAMまでマップする.
    kvmmap(kpgtbl, (uint64_t)etext, (uint64_t)etext, PHYSTOP-(uint64_t)etext, PTE_NORMAL);

    // トラップ処理への入出力ようにトランポリンをカーネルの最大仮想アドレスにマップする
    kvmmap(kpgtbl, TRAMPOLINE, (uint64_t)trampoline, PGSIZE, PTE_EXEC);

    // 各プロセス用のカーネルスタックを割り当ててマップする.
    // プロすす数の上限は決まっており、すべて割り当てる。
    proc_mapstacks(kpgtbl);

    return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void)
{
    kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart(void)
{
    // wait for any previous writes to the page table memory to finish.
    asm volatile("fence rw, rw");
    sfence_vma();

    w_satp(MAKE_SATP(kernel_pagetable));

    // flush stale entries from the TLB.
    sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64_t va, int alloc)
{
    if (va >= MAXVA)
        panic("walk");

    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            if (!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
                return 0;
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

// 仮想アドレスを検索して物理アドレスを返す。
// マップされていない場合は0を返す。
// ユーザページの検索にしか使用できない.
uint64_t
walkaddr(pagetable_t pagetable, uint64_t va)
{
    pte_t *pte;
    uint64_t pa;

    if (va >= MAXVA)
        return 0;

    pte = walk(pagetable, va, 0);
    if (pte == 0)
        return 0;
    if ((*pte & PTE_V) == 0)
        return 0;
    if ((*pte & PTE_U) == 0)
        return 0;
    pa = PTE2PA(*pte);
    return pa;
}

// カーネルページテーブルにマッピングを追加する.
// 起動時にのみ使用される。
// TLBをflushしたり、ページングを有効にしてはならない.
void
kvmmap(pagetable_t kpgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm)
{
    if (mappages(kpgtbl, va, sz, pa, perm) != 0)
        panic("kvmmap");
}

// paから始まる物理アドレスを参照するvaから始まる仮想アドレスのPTEを作成する.
// vaとsizeはページアラインされていない場合がある。
// 成功したら 0, walk()が必要なページテーブルページを
// 割り当てられなかった場合は -1 を返す。
int
mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, uint64_t perm)
{
    uint64_t a, last;
    pte_t *pte;

    if (size == 0)
        panic("mappages: size");

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for(;;) {
        if((pte = walk(pagetable, a, 1)) == 0)
            return -1;
        if (*pte & PTE_V) {
            debug("va: 0x%lx, pte: 0x%lx, *pte=0x%lx", a, pte, *pte);
            panic("mappages: remap");
        }

        *pte = PA2PTE(pa) | perm | PTE_V | PTE_A | PTE_D;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    sfence_vma();
    return 0;
}

// vaから始まるマッピングをnpagesページ削除する。vaはページアライン
// されていなければならない。マッピングは存在しなければならない。
// オプションで物理メモリを解放する。
void
uvmunmap(pagetable_t pagetable, uint64_t va, uint64_t npages, int do_free)
{
    uint64_t a;
    pte_t *pte;

    if ((va % PGSIZE) != 0)
        panic("uvmunmap: not aligned");

    for (a = va; a < va + npages*PGSIZE; a += PGSIZE) {
        if ((pte = walk(pagetable, a, 0)) == 0) {
            trace("no pte for va: 0x%lx", a);
            continue;
        }

        // uvmcopyと同じ理由
        if ((*pte & PTE_V) == 0) {
            trace("[%d] pte: %p, *pte: 0x%lx", a, pte, *pte);
            continue;
        }
        if ((PTE_FLAGS(*pte) & 0x3ff) == PTE_V)
            panic("uvmunmap: not a leaf");
        if (do_free) {
            uint64_t pa = PTE2PA(*pte);
            trace("pa: 0x%lx, refcnt: %d", pa, page_refcnt_get((char *)pa));
            kfree((void*)pa);
        }
        *pte = 0;
    }
    sfence_vma();
    fence_i();
}

// 空のユーザページテーブルを作成する.
// メモリ不足の場合は 0 を返す.
pagetable_t
uvmcreate(void)
{
    pagetable_t pagetable;
    pagetable = (pagetable_t) kalloc();
    if (pagetable == 0)
        return 0;
    memset(pagetable, 0, PGSIZE);
    return pagetable;
}

// ユーザモードinitcodeを最初のプロセスとして
// pagetableのアドレス0にロードする。szは1ページ
// 未満でなければならない
void
uvmfirst(pagetable_t pagetable, uchar *src, uint32_t sz)
{
    char *mem;

    if (sz >= PGSIZE)
        panic("uvmfirst: more than a page");
    mem = kalloc();
    memset(mem, 0, PGSIZE);
    mappages(pagetable, 0, PGSIZE, (uint64_t)mem, PTE_NORMAL|PTE_X|PTE_U);
    memmove(mem, src, sz);
}

// oldszからnewsz（いずれもページにアラインしている必要はない）までプロセス
// サイズを増加させるためにPTEと物理メモリを割り当てる。 新しいサイズを返すか、
// エラーの場合は 0 を返す.（デフォルトはRO/Xの実行コード用, xpermでwriteを追加可能）
uint64_t
uvmalloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz, int xperm)
{
    char *mem;
    uint64_t a;

    if (newsz < oldsz)
        return oldsz;

    oldsz = PGROUNDUP(oldsz);
    for (a = oldsz; a < newsz; a += PGSIZE){
        mem = kalloc();
        if (mem == 0) {
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages(pagetable, a, PGSIZE, (uint64_t)mem, PTE_RO|PTE_U|xperm) != 0) {
            kfree(mem);
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
        trace("mapped: 0x%lx - 0x%lx : %p", a, a + PGSIZE, mem);
    }
    return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64_t
uvmdealloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz)
{
    if (newsz >= oldsz)
        return oldsz;

    if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
    }

    return newsz;
}

// 再帰的にページテーブルページを解放する.
// リーフマッピングはすべて事前に削除されていなければならない.
static void
freewalk(pagetable_t pagetable)
{
    // ページテーブルには 2^9 = 512 個のPTEがある.
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
            // このPTEは下位レベルのページテーブルを指している.
            uint64_t child = PTE2PA(pte);
            freewalk((pagetable_t)child);
            pagetable[i] = 0;
        } else if (pte & PTE_V) {
            error("leaf for pa: 0x%lx is not removed", PTE2PA(pte));
            panic("Invalid leaf PTE");
        }
    }
    kfree((void*)pagetable);
}

// ユーザメモリページを解放し、
// 次に、ページテーブルページを解放する.
void
uvmfree(pagetable_t pagetable, uint64_t sz)
{
    if (sz > 0)
        uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
    freewalk(pagetable);
}

// 親プロセスのページテーブルを子のページテーブルにコピーする。
// 物理メモリもコピーする.
// 成功したら 0, 失敗したら -1 を返す.
// 失敗した場合は割り当てたページをすべて開放する.
int
uvmcopy(struct proc *old, struct proc *new)
{
    pte_t *pte_2, *pte_1, *pte_0;
    pagetable_t pg1, pg0;
    uint64_t pa, va;
    uint64_t flags;
    char *mem;
    struct mmap_region *region;

    for (int i = 0; i < 256; i++) {
        pte_2 = &old->pagetable[i];
        if (*pte_2 & PTE_V) {
            pg1 = (pagetable_t)PTE2PA(*pte_2);
            for (int j = 0; j < 512; j++) {
                pte_1 = &pg1[j];
                if (*pte_1 & PTE_V) {
                    pg0 = (pagetable_t)PTE2PA(*pte_1);
                    for (int k = 0; k < 512; k++) {
                        // MAXVA - USERTOP : trampoline, trapframce, guardpageはmapping済み
                        if (i == 255 && j == 511 && k >= 509)
                            continue;
                        pte_0 = &pg0[k];
                        if (*pte_0 & PTE_V) {
                            va = (uint64_t)i << 30 | (uint64_t)j << 21 | (uint64_t)k << 12;
                            pa = PTE2PA(*pte_0);
                            flags = PTE_FLAGS(*pte_0);
                            region = find_mmap_region(old, (void *)va);
                            // mmapされたアドレスでMAP_SHAREDの場合は親のpaをそのまま使用
                            // それ以外は新規paに親のpaをコピーして使用
                            if (region && region->flags & MAP_SHARED) {
                                mem = (char *)pa;
                                page_refcnt_inc((void *)pa);
                                trace("use parent's pa: 0x%lx, refcnt: %d", pa, page_refcnt_get((void *)pa));
                            } else {
                                if ((mem = kalloc()) == 0)
                                    goto err;
                                memmove(mem, (char*)pa, PGSIZE);
                            }
                            if (mappages(new->pagetable, va, PGSIZE, (uint64_t)mem, flags) != 0) {
                                goto err;
                            }
                        }
                    }
                }
            }
        }
    }

    fence_i();
    fence_rw();

    return 0;

err:
    if (va < MMAPBASE)
        uvmunmap(new->pagetable, 0, va / PGSIZE, 1);
    else
        uvmunmap(new->pagetable, 0, (va - MMAPBASE) / PGSIZE, 1);
    return -1;

}

// ユーザーアクセスに対してPTEを無効とマークする。
// execがユーザスタックのガードページとして使用する。
void
uvmclear(pagetable_t pagetable, uint64_t va)
{
    pte_t *pte;

    pte = walk(pagetable, va, 0);
    if (pte == 0)
        panic("uvmclear");
    *pte &= ~PTE_U;
}

// カーネル空間からユーザ空間にコピーする.
// srcから指定されたページテーブルの仮想アドレスdstvaにlenバイトコピーする.
// 成功したら0を返し、エラーなら-1を返す。
int
copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len)
{
    uint64_t n, va0, pa0;

    while (len > 0) {
        va0 = PGROUNDDOWN(dstva);
        int ret = alloc_cow_page(pagetable, va0);
        if (ret < 0) {
            return -1;
        } else if (ret == 1) {
            trace("not cow");
        }

        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0) {
            trace("pa0 = 0: dstva: 0x%lx (va0: 0x%lx)", dstva, va0);
            return -1;
        }

        n = PGSIZE - (dstva - va0);
        if (n > len)
            n = len;
        trace("memmove: dst: 0x%lx (pa0: 0x%lx + dstva: 0x%lx - va0: 0x%lx), src: %p, n: %ld", pa0 + (dstva - va0), pa0, dstva, va0, src, n);
        if (memmove((void *)(pa0 + (dstva - va0)), src, n) < 0) {
            warn("memmove error: dst: 0x%lx (pa0: 0x%lx + dstva: 0x%lx - va0: 0x%lx), src: %p, n: %ld", pa0 + (dstva - va0), pa0, dstva, va0, src, n);
            return -1;
        }

        len -= n;
        src += n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

// ユーザからカーネルにコピーする。
// 与えられたページテーブルの仮想アドレスsrcvaからカーネル変数dstに
// lenバイトコピーする。
// 成功したら0を返し、エラーなら-1を返す。
int copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len)
{
    trace("dst: %p, srcva: 0x%lx, len: %ld", dst, srcva, len);
    uint64_t n, va0, pa0;

    while (len > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0) {
            trace("pa0 = 0: srcva: 0x%lx (va0: 0x%lx)", srcva, va0);
            return -1;
        }
        n = PGSIZE - (srcva - va0);
        if (n > len)
            n = len;
        if (memmove(dst, (void *)(pa0 + (srcva - va0)), n) < 0) {
            warn("memmove error: dst: %p, src: 0x%lx, n: %ld, (pa0: 0x%lx + srcva: 0x%lx - va0: 0x%lx)", dst, pa0 + (srcva - va0), n, pa0, srcva, va0);
            return -1;
        }
        len -= n;
        dst += n;
        srcva = va0 + PGSIZE;
    }
    return 0;
}

// null終端の文字列をユーザ空間からカーネル空間にコピーする.
// 指定のページテーブルにある仮想アドレス srcvaからdstに'\0'が
// 現れるかmaxサイズになるまでバイトをコピーする,
// 成功したら 0, エラーが発生したら -1 を返す.
int
copyinstr(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t max)
{
    uint64_t n = 0, va0, pa0;
    int got_null = 0;
    trace("pid[%d] dst=%p, srcva=0x%lx, max=%ld", myproc()->pid, dst, srcva, max);

    if (srcva == 0UL) {
        // 空送信
        //*dst = '\0';
        return 0;
    }

    while (got_null == 0 && max > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0) {
            trace("va0: 0x%lx, pa0=0", va0);
            return -1;
        }

        n = PGSIZE - (srcva - va0);
        if (n > max)
            n = max;

        char *p = (char *) (pa0 + (srcva - va0));
        while (n > 0) {
            if (*p == '\0') {
                *dst = '\0';
                got_null = 1;
                break;
            } else {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }

        srcva = va0 + PGSIZE;
    }
    if (got_null) {
        return 0;
    } else {
        return -1;
    }
}

int alloc_cow_page(pagetable_t pagetable, uint64_t va)
{
    if (va >= MAXVA)
        return -1;

    pte_t *pte = walk(pagetable, va, 0);
    // COW領域でない
    if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
        return 1;
    }

    // COW領域ではない書き込み不可アドレスの書き込み例外: プロセスをkill
    if (!(*pte & PTE_COW) && !(*pte & PTE_W)) {
        debug("*pte DAGU_XWRV = %08b", *pte & 0xff);
        return -1;
    }

    // COW領域ではない書き込み可アドレスの書き込み例外: ここでは何もしない
    if ((*pte & PTE_W) || !(*pte & PTE_COW)) {
        trace("pid[%d] va: 0x%lx not COW", myproc()->pid)
        return 1;
    }

    uint64_t flags = PTE_FLAGS(*pte);
    uint64_t pa = PTE2PA(*pte);
    uint64_t va0 = PGROUNDDOWN(va);
    char *mem;

    // 複数のプロセスがこのページを参照しているのでコピーが必要
    if (page_refcnt_get((void *) pa) > 1) {
        if ((mem = kalloc()) == 0) {
            error("no memory");
            return -1;
        }
        memmove(mem, (char*)pa, PGSIZE);
        uvmunmap(pagetable, va0, 1, 1);
        flags &= ~PTE_COW;
        flags |= PTE_W;
        if (mappages(pagetable, va0, PGSIZE, (uint64_t)mem, flags) != 0) {
            kfree(mem);
            error("mappages error: va=0x%lx", va);
            return -1;
        }
        trace("alloc ok: va=0x%lx, pa: %p", va, mem);
        fence_i();
        return 0;
    } else if (page_refcnt_get((void *) pa) == 1){
        *pte |= PTE_W;
        *pte &= ~PTE_C;
        trace("flag updated: pa: 0x%lx", pa);
        fence_i();
        return 0;
    } else {
        error("unknown error: va=0x%lx", va);
        return -1;
    }
}

void uvmdump(pagetable_t pagetable, pid_t pid, char *name)
{
    printf("\npid[%d] name: %s, pagetable: %p\n", pid, name, pagetable);

    for (int i = 0; i < 512; i++) {
        pte_t *pte_2 = &pagetable[i];
        if (*pte_2 & PTE_V) {
            pagetable_t pg1 = (pagetable_t)PTE2PA(*pte_2);
            for (int j = 0; j < 512; j++) {
                pte_t *pte_1 = &pg1[j];
                if (*pte_1 & PTE_V) {
                    pagetable_t pg0 = (pagetable_t)PTE2PA(*pte_1);
                    uint64_t pa = PTE2PA(*pte_1);
                    uint64_t va = (uint64_t)i << 30 | (uint64_t)j << 21;
                    printf("  [% 3d][% 3d]   va: 0x%010lx, pa: 0x%010lx\n", i, j, va, pa);
                    for (int k = 0; k < 512; k++) {
                        pte_t *pte_0 = &pg0[k];
                        if (*pte_0 & PTE_V) {
                            uint64_t va = (uint64_t)i << 30 | (uint64_t)j << 21 | (uint64_t)k << 12;
                            printf("            [% 3d] [0x%010lx - 0x%010lx) : DAGU_XWRV = %08b\n", k, va, va + PGSIZE, *pte_0 & 0xff);
                        }
                    }
                }
            }
        }
    }
}
