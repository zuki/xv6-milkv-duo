#include <common/param.h>
#include <common/types.h>
#include <common/memlayout.h>
#include <elf.h>
#include <common/riscv.h>
#include <defs.h>
#include <common/fs.h>
#include <config.h>
#include <cv180x_reg.h>
#include <printf.h>

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

    // uart registers
    kvmmap(kpgtbl, UART0, UART0_PHY, PGSIZE, PTE_DEVICE);

    kvmmap(kpgtbl, MMIO_BASE, MMIO_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, CLKGEN_BASE, CLKGEN_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, PINMUX_BASE, PINMUX_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, RESET_BASE, RESET_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, RTC_CTRL_BASE, RTC_CTRL_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, RTC_CORE_BASE, RTC_CORE_BASE, PGSIZE, PTE_DEVICE);
    kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_DEVICE);
    kvmmap(kpgtbl, SD0_BASE, SD0_BASE, 16*PGSIZE, PTE_DEVICE);

    // map kernel text executable and read-only.
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64_t)etext-KERNBASE, PTE_EXEC);

    // map kernel data and the physical RAM we'll make use of.
    kvmmap(kpgtbl, (uint64_t)etext, (uint64_t)etext, PHYSTOP-(uint64_t)etext, PTE_NORMAL);

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel.
    kvmmap(kpgtbl, TRAMPOLINE, (uint64_t)trampoline, PGSIZE, PTE_EXEC);

    // allocate and map a kernel stack for each process.
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
            if (!alloc || (pagetable = (pde_t*)kalloc()) == 0)
                return 0;
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
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

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm)
{
    if(mappages(kpgtbl, va, sz, pa, perm) != 0)
        panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
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
        if (*pte & PTE_V)
            panic("mappages: remap");
        *pte = PA2PTE(pa) | perm | PTE_V | PTE_A | PTE_D;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    sfence_vma();
    return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64_t va, uint64_t npages, int do_free)
{
    uint64_t a;
    pte_t *pte;

    if ((va % PGSIZE) != 0)
        panic("uvmunmap: not aligned");

    for (a = va; a < va + npages*PGSIZE; a += PGSIZE) {
        if ((pte = walk(pagetable, a, 0)) == 0)
            panic("uvmunmap: walk");
        if ((*pte & PTE_V) == 0)
            panic("uvmunmap: not mapped");
        if ((PTE_FLAGS(*pte) & 0x3ff) == PTE_V)
            panic("uvmunmap: not a leaf");
        if (do_free) {
            uint64_t pa = PTE2PA(*pte);
            kfree((void*)pa);
        }
        *pte = 0;
    }
    sfence_vma();
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
        trace("mapped: 0x%lx - 0x%lx", a, a + PGSIZE);
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

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
static void
freewalk(pagetable_t pagetable)
{
    // there are 2^9 = 512 PTEs in a page table.
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
            // this PTE points to a lower-level page table.
            uint64_t child = PTE2PA(pte);
            freewalk((pagetable_t)child);
            pagetable[i] = 0;
        } else if(pte & PTE_V) {
            panic("freewalk: leaf");
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

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64_t sz)
{
    pte_t *pte;
    uint64_t pa, i;
    uint64_t flags;
    char *mem;

    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walk(old, i, 0)) == 0)
            panic("uvmcopy: pte should exist");
        if ((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = kalloc()) == 0)
            goto err;
        memmove(mem, (char*)pa, PGSIZE);
        if (mappages(new, i, PGSIZE, (uint64_t)mem, flags) != 0) {
            kfree(mem);
            goto err;
        }
    }
    // Synchronize the instruction and data streams,
    // since we may copy pages with instructions.
    fence_i();
    return 0;

    err:
    uvmunmap(new, 0, i / PGSIZE, 1);
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
// 与えられたページテーブルの仮想アドレスsrcvaからdstに
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
    uint64_t n, va0, pa0;
    int got_null = 0;

    while (got_null == 0 && max > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0)
            return -1;
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
                            printf("            [% 3d] [0x%010lx - 0x%010lx)\n", k, va, va + PGSIZE);
                        }
                    }
                }
            }
        }
    }
}
