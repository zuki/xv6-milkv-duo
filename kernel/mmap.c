#include <common/types.h>
#include <common/riscv.h>
#include <defs.h>
#include <common/file.h>
#include <common/memlayout.h>
#include <errno.h>
#include <printf.h>
#include <proc.h>
#include <spinlock.h>
#include <linux/fcntl.h>
#include <linux/mman.h>

/* utils */
#define NOT_PAGEALIGN(a)  ((uint64_t)(a) & (PGSIZE-1))

/*
 * struct mmap_regionリンクリストからnodeを削除して、node->nextをprevに
 * つなぐ。munmap()が呼ばれた時に呼び出される
 */
static void delete_mmap_node(struct proc *p, struct mmap_region *node)
{
    if (p->regions == NULL) return;

    //trace("delete_mmap_node[%d]: addr=%p", p->pid, node->addr);

    //print_mmap_list(p, "delete node before");

    struct mmap_region *region, *prev;
    // nodeがp->regionsの先頭region
    if (node->addr == p->regions->addr) {
        if (p->regions->next != NULL)
            p->regions = p->regions->next;
        else
            p->regions = NULL;
    } else {
        region = prev = p->regions;
        while (region) {
            if (node->addr == region->addr) {
                if (region->next != NULL)
                    prev->next = region->next;
                else
                    prev->next = NULL;
                break;
            }
            prev = region;
            region = region->next;
        }
    }
    trace("uvmunmp: pid=%d, addr=%p", p->pid, node->addr);
    uvmunmap(p->pagetable, (uint64_t)node->addr, (((uint64_t)node->length + PGSIZE - 1) / PGSIZE), 1);
    slab_cache_free(MMAPREGIONS, node);
    node = NULL;

    //print_mmap_list(p, "delete node after");
}

/*
 * struct mmap_regionリンクリスト全体をクリアする。
 * ユーザ空間にあるメモリページをすべて開放しなければ
 * ならない時にexecから呼び出される
 */
void free_mmap_list(struct proc *p)
{
    struct mmap_region* region = p->regions;
    struct mmap_region* temp;

    while (region) {
        temp = region;
        if (region->f) {
            fileclose(region->f);
        }
        delete_mmap_node(p, region);
        region = temp->next;
    }
}

// srcからdestにmmap_regionをコピー
static void copy_mmap_region(struct mmap_region *dest, struct mmap_region *src)
{
    dest->addr          = src->addr;
    dest->length        = src->length;
    dest->flags         = src->flags;
    dest->prot          = src->prot;
    dest->offset        = src->offset;
    dest->next          = NULL;

    if (!(src->flags & MAP_ANONYMOUS) && src->f) {
        dest->f = filedup(src->f);
    } else {
        dest->f = NULL;
    }
}

// 指定されたprotectionからpermを作成する
uint64_t get_perm(int prot, int flags)
{
    uint64_t perm;
    if (flags & MAP_ANONYMOUS)
        perm = PTE_THEAD_NORMAL_NC | PTE_U;
    else
        perm = PTE_THEAD_NORMAL | PTE_U;

    if (prot & PROT_READ)
        perm |= PTE_R;
    if (prot & PROT_WRITE)
        perm |= PTE_W;
    if (prot & PROT_EXEC)
        perm |= PTE_X;
    if (prot & PROT_NONE)
        perm &= ~PTE_U;

    return perm;
}

/*
 * addr, lengthを持つmmap_regionを作成できるか
 * できる: 1, できない: 0
 */
static int is_usable(void *addr, size_t length)
{
    struct proc *p = myproc();

    // ELF_ET_DYN_BASE未満のアドレスは使用不可
    if (addr < (void *)ELF_ET_DYN_BASE || addr > (void *)USERTOP)
        return 0;

    struct mmap_region *cursor = p->regions;
    // regionが一つも作成されていなければ使用可能
    if (cursor == NULL)
        return 1;

    while (cursor) {
        // 1. 使用済み
        if (addr == cursor->addr)
            return 0;
        // 2: 右端が最左のregionより小さい
        if (addr + length <= cursor->addr)
            return 1;
        // 3: 左端が最左のregionより大きい、かつ、次のregionがないか、右端が次のregionより小さい
        if (cursor->addr + cursor->length <= addr && (cursor->next == 0 || addr + length <= cursor->next->addr))
            return 1;
        cursor = cursor->next;
    }

    return 0;
}

/*
 * offsetからファイルの内容をaddrにlengthマッピングする
 */
static long map_file_pages(struct proc *p, void *addr, uint64_t length, uint64_t perm, struct file *f, off_t offset)
{
    long ret;
    uint64_t cur;

    for (cur = 0; cur < length; cur += PGSIZE) {
        f->off = offset;
        int len = (f->ip->size - f->off) > PGSIZE ? PGSIZE : (f->ip->size - f->off);
        if (len == 0)
            break;
        //trace("addr=%p, length=0x%x, offset=0x%x", addr, length, offset);
        char *mem = kalloc();
        if (!mem) {
            ret = -ENOMEM;
            goto err;
        }
        memset(mem, 0, PGSIZE);
        if ((ret = fileread(f, (uint64_t)mem, len, 0)) < 0) {
            error("fileread failed");
            kfree(mem);
            goto err;
        }

        // メモリをユーザプロセスにマッピング
        if (p->pid == 7)
            trace("pid[%d] mapping: addr=%p, mem=%p, offset: 0x%x, len: 0x%x", p->pid, addr+cur, mem, offset, len);
        if ((ret = mappages(p->pagetable, (uint64_t)addr + cur, (uint64_t)len, (uint64_t)mem, perm)) < 0) {
            kfree(mem);
            return ret;
        }
        offset += len;
    }

    fence_i();
    fence_rw();

    return 0;

err:
    if (cur != 0) {
        uvmunmap(p->pagetable, (uint64_t)addr, cur / PGSIZE, 1);
    }
    return ret;
}

// 無名ページに（複数）ページを割り当てる
static long map_anon_pages(struct proc *p, void *addr, uint64_t length, uint64_t perm)
{
    long ret;
    uint64_t cur;

    for (cur = 0; cur < length; cur += PGSIZE) {
        char *page = kalloc();
        if (!page) {
            error("map_anon_page: memory exhausted");
            ret = -ENOMEM;
            goto err;
        }
        memset(page, 0, PGSIZE);
        if (p->pid == 7)
            trace("pid[%d] map addr %p to page %p with perm 0x%lx", p->pid, addr+cur, page, perm);
        if (mappages(p->pagetable, (uint64_t)addr + cur, PGSIZE, (uint64_t)page, perm) < 0) {
            kfree(page);
            ret = -EINVAL;
            goto err;
        }
    }

    fence_i();
    fence_rw();

    return 0;

err:
    if (cur != 0) {
        uvmunmap(p->pagetable, (uint64_t)addr, cur / PGSIZE, 1);
    }
    return ret;
}

/* addrにメモリを割り当てる */
long mmap_load_pages(void *addr, uint64_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct proc *p = myproc();

    uint64_t perm = get_perm(prot, flags);
    //if (flags & MAP_SHARED) perm |= PTE_W;

    if (flags & MAP_ANONYMOUS)
        return map_anon_pages(p, addr, length, perm);
    else
        return map_file_pages(p, addr, length, perm, f, offset);
}


// regionのサイズをsizeにする（拡大する場合は、拡大可能であることが保証されていること）
// sizeはPGSIZEの整数倍とする
static long scale_mmap_region(struct mmap_region *region, uint64_t size)
{
    long error;

    if (region->length == size) return 0;

    if (region->length < size) { // 拡大
        if ((error = mmap_load_pages(region->addr + region->length, size - region->length, region->prot, region->flags, region->f, region->offset)) < 0)
            return error;
    } else {                        // 縮小
        uint64_t dstart = PGROUNDDOWN((uint64_t)region->addr + region->length);
        uint64_t dpages = (region->length - dstart + PGSIZE - 1) / PGSIZE;
        //int free = (region->flags & MAP_SHARED) ? 0 : 1;
        if ((uint64_t)region->addr + size <= dstart && dpages > 0) {
            uvmunmap(myproc()->pagetable, dstart, dpages, 1);
        }
    }
    region->length = size;

    return 0;
}

// struct mmap_region listを出力
void print_mmap_list(struct proc *p, const char *title)
{
    printf("[INFO] pid[%d]: mmap_region list (%s) at %p\n", p->pid, title, p->regions);

    struct mmap_region *region = p->regions;
    int i=0;
    while (region) {
        printf(" - region[%d]: addr=%p, length=0x%x, prot=0x%x, flags=0x%x, f=%d, offset=0x%x\n",
            ++i, region->addr, region->length, region->prot, region->flags, (region->f ? region->f->ip->inum : 0), region->offset);
        region = region->next;
    }
}

// 親プロセスから子プロセスにmmap_regionをコピー.
long copy_mmap_regions(struct proc *parent, struct proc *child)
{
    struct mmap_region *node = parent->regions;
    struct mmap_region *cnode = NULL, *tail = 0;

    while (node) {
        struct mmap_region *region = slab_cache_alloc(MMAPREGIONS);
        if (region == (struct mmap_region *)0)
            return -ENOMEM;

        // mmap_regionのコピー
        copy_mmap_region(region, node);

        if (cnode == 0)
            cnode = region;
        else
            tail->next = region;

        tail = region;
        node = node->next;
    }

    child->regions = cnode;

    return 0;
}

/* startを含むregionを探す */
struct mmap_region *find_mmap_region(struct proc *p, void *start)
{
    struct mmap_region *region = p->regions;

    while (region) {
        if (region->addr <= start && start < (region->addr + region->length))
            return region;
        region = region->next;
    }
    return NULL;
}

/* addr + length はp->regionsに含まれるか */
bool is_mmap_region(struct proc *p, void *addr, uint64_t length)
{
    struct mmap_region *region = find_mmap_region(p, addr);
    if (region == NULL)
        return 0;
    return ((uint64_t)addr + length <= (uint64_t)region->addr + region->length);
}

// 遅延mapを実装（trap.cから呼び出される）
// 該当するアドレスを含む1ページ分の割り当て/ファイル読み込みをする
long alloc_mmap_page(struct proc *p, uint64_t addr,  uint64_t scause) {
    off_t offset = 0;

    uint64_t rounddown = PGROUNDDOWN(addr);
    struct mmap_region *region = find_mmap_region(p, (void *)rounddown);
    if (region == NULL) {
        error("no region with addr 0x%lx", addr);
        return -1;
    }

    if (scause == SCAUSE_PAGE_STORE && (region->prot & PROT_WRITE) == 0) {
        error("region is not writable");
        return -1;
    }

    // ファイルオフセットの計算
    if (region->f) {
        offset = region->offset + ((rounddown - (uint64_t)region->addr) / PGSIZE) * PGSIZE;
    }
    trace("pid[%d] addr: 0x%lx, rounddown: 0x%lx, offset: %ld", p->pid, addr, rounddown, offset);
    if (mmap_load_pages((void *)rounddown, PGSIZE, region->prot, region->flags, region->f, offset) < 0) {
        error("loading page failed: addr: 0x%lx, length: 0x%x, prot: 0x%x, flags: 0x%x, f: %d, offset: 0x%x",
            rounddown, PGSIZE, region->prot, region->flags, region->f ? region->f->ip->inum : -1, offset);
        return -1;
    }
    return 0;
}

// sys_mmapのメイン関数
long mmap(void *addr, size_t length, int prot, int flags, struct file *f, off_t offset)
{
    struct proc *p = myproc();
    struct mmap_region *node;
    long error = -EINVAL;

    if (p->pid == 7)
        trace("addr: %p, length: 0x%lx, prot: 0x%x, flags: 0x%x, f: %d, off: 0x%lx",
        addr, length, prot, flags, f ? f->ip->inum : 0, offset);

    // MAP_FIXEDの指定アドレスはページ境界にあり、割り当て領域がMMAPエリア内に入ること
    // 1. addrを確定する
    // 1.1. アドレスが指定されている場合
    if (addr != 0) {
        // 1.1.1 指定アドレス+指定サイズはUSERTOPを超えないこと
        // upper_addr: addr + sizeがMMAPTOPを超えないかチェックするための変数
        uint64_t upper_addr = PGROUNDUP(PGROUNDUP((uint64_t)addr) + length);
        if (upper_addr > USERTOP) {
            error("addr: %p + length: 0x%lx overs USERTOP", addr, length);
            return -EINVAL;
        }
        // 1.1.2 MAP_FIXEDが指定されている場合
        if (flags & MAP_FIXED) {
            // 1.1.2.1 アドレスはページ境界にあること
            if (NOT_PAGEALIGN(addr)) {
                error("fixed address should be page align: %p", addr);
                return -EINVAL;
            }
            if (prot == PROT_NONE) {
                for (int i = 0; i < PGROUNDUP(length) / PGSIZE; i++) {
                    uint64_t *pte = walk(p->pagetable, (uint64_t)addr + i * PGSIZE, 1);
                    if (pte == NULL) {
                        error("PROT_NONE invalid addr");
                        return -EINVAL;
                    }
                    // ユーザアクセスを禁止にする
                    *pte &= ~PTE_U;
                }
                return (long)addr;
            } else {
                // 1.1.2.2 指定されたアドレスが使用可能なこと
                if (!is_usable(addr, length)) {
                    error("addr %p is not available", addr);
                    return -EINVAL;
                }
            }
        // 1.1.3 MAP_FIXEDが指定されていない場合は、アドレスを丸め下げ
        } else {
            addr = (void *)PGROUNDDOWN((uint64_t)addr);
            goto select_addr;
        }
    // 1.2. アドレスが指定されていない場合
    } else {
        // 1.2.1 最初のアドレス候補
        if (p->regions)
            addr = p->regions->addr;
        else
            addr = (void *)MMAPBASE;
select_addr:
        node = p->regions;
        while (node) {
            trace("- addr=0x%x, node->addr=0x%x, node->next->addr=0x%x", addr, node->addr, node->next ? node->next->addr : NULL);
            // 1.2.31 作成マッピングが現在のノードアドレスより小さい場合はこの候補を使用する
            if (addr + PGROUNDUP(length) <= node->addr)
                break;
            // 1.2.2 次のマッピングがない、または次のマッピングとの間に置ける場合はこの候補を使用する
            if (node->addr + node->length <= addr && (node->next == 0 || addr + PGROUNDUP(length) <= node->next->addr))
                break;
            // 1.2.5 それ以外は、現在のマッピングの右端をアドレス候補とする
            if (addr <= node->addr + node->length)
                addr = node->addr + node->length;
            // 1.2.6 次のマッピングと比較する
            node = node->next;
        }
    }
    // 1.3 決定したアドレスがマップ範囲に含まれていることをチェックする
    if (addr + PGROUNDUP(length) > (void *)USERTOP)
        return -ENOMEM;

    // 2. 新規mmap_regionを作成する
    // 2.1 mmap_regionのためのメモリを割り当てる
    struct mmap_region *region = slab_cache_alloc(MMAPREGIONS);
    if (region == NULL)
        return -ENOMEM;
    // 2.2 mmap_regionにデータを設定する
    region->addr   = addr;
    region->length = length;
    region->flags  = flags;
    region->offset = offset;
    region->prot   = prot;
    region->next   = NULL;

    // 2.2 fを設定
    if (f) {
        filedup(f);
        if (!(flags & MAP_ANONYMOUS)) {
            region->f = filedup(f);  // munmapとofile-closeで2回decrementされる
        } else {
            goto out;
        }
    } else {
        region->f = NULL;
    }

    // 3. p->regionsに作成したmmap_regionを追加する
    // 3.1 これがプロセスの最初のmmap_regionの場合はp->regionsに追加する
    if (p->regions == NULL) {
        p->regions = region;
        goto load_pages;
    }

    // 3.2 そうでない場合は適切な位置に追加して、p->regionsを更新する
    node = p->regions;
    struct mmap_region *prev = p->regions;
    while (node) {
        //cprintf("addr=%p, node->addr=%p\n", addr, node->addr);
        if (addr < node->addr) {
            region->next = node;
            prev = region;
            break;
        } else if (addr > node->addr && node->next && addr < node->next->addr) {
            region->next = node->next;
            node->next = region;
            break;
        }
        if (!node->next) {
            node->next = region;
            break;
        }
        node = node->next;
    }
    p->regions = prev;

load_pages:
    if ((error = mmap_load_pages(addr, length, prot, flags, f, offset)) < 0)
        goto out;

    region->addr = addr;
    // ファイルオフセットを正しく処理するためにlengthはここで切り上げる
    region->length = PGROUNDUP(length);
    trace("return addr: %p, length: 0x%lx", region->addr, region->length);
    return (long)region->addr;

out:
    if (f) fileclose(f);
    slab_cache_free(MMAPREGIONS, region);
    return error;
}

// sys_munmapのメイン関数
long munmap(void *addr, size_t length)
{
    struct proc *p = myproc();

    // addrはページ境界になければならない
    if ((uint64_t)addr & (PGSIZE - 1))
        return -EINVAL;
    // lengthは境界になくてもよいが、処理は境界に合わせる
    length = PGROUNDUP(length);

    trace("pid[%d] addr=%p, length=0x%x", p->pid, addr, length);

    struct mmap_region *region = find_mmap_region(p, addr);
    if (region == NULL) {
        warn("no region with addr %p", addr);
        return 0;
    }

    if (region->length < length) {
        warn("length 0x%lx is bigger than region->length 0x%lx", length, region->length);
        length = region->length;
    }
    trace(" - found: addr=%p", region->addr);
    // MAP_SHARED領域で背後にあるファイルに書き込みがあったら書き戻す
    //int len = (f->ip->size - f->off) > PGSIZE ? PGSIZE : (f->ip->size - f->off);
    if ((region->flags & MAP_ANONYMOUS) == 0 && region->flags & MAP_SHARED && region->prot & PROT_WRITE) {
        for (uint64_t ra = (uint64_t)addr; ra < (uint64_t)addr + length; ra += PGSIZE) {
            pte_t *pte = walk(p->pagetable, ra, 0);
            if (!pte) panic("no pte");
            uint64_t flags = PTE_FLAGS(*pte);
            off_t offset = region->offset + ra - (uint64_t)region->addr;
            if (flags & PTE_D) {
                if (writeback(region->f, offset, ra) < 0) {
                    error("failed writeback");
                    return -EACCES;
                }
            }
            if (region->f->ip->size <= offset + PGSIZE)
                break;
        }
    }

    // mmap_regionを全部削除
    if (region->length == length) {
        trace("delete WHOLE");
        if (region->f) {
            fileclose(region->f);
        }
        delete_mmap_node(p, region);
    // mmap_regionの一部を削除
    } else {
        trace("delete PART");
        uvmunmap(p->pagetable, (uint64_t)addr, length / PGSIZE, 1);
        if (region->addr == addr) {
            region->addr += length;
            region->offset += length;
        }
        region->length -= length;
        trace("new region: addr=%p, length=0x%x", region->addr, region->length);
    }
    //print_mmap_list(p, "munmap");
    //uvmdump(p->pagetable, p->pid, "munmap");
    return 0;
}

void *mremap(void *old_addr, size_t old_length, size_t new_length, int flags, void *new_addr)
{
    void *mapped_addr;
    long error = -EINVAL;
    //trace("- remap: old_addr=%p, old_length=0x%x, new_length=0x%x, flags=0x%x",
    //    old_addr, old_length, new_addr, flags);
    struct mmap_region *region = find_mmap_region(myproc(), old_addr);
    if (region == NULL) return (void *)error;

    if (region->length != old_length) return  (void *)error;

    new_length = PGROUNDUP(new_length);

    // 1: その場で拡大（縮小）可能の場合
    if (!region->next || region->addr + new_length <= region->next->addr) {
        if (flags & MREMAP_FIXED) {
            if ((error = munmap(old_addr, old_length)) < 0)
                return  (void *)error;
            return (void *)mmap(new_addr, new_length, region->prot, (region->flags | MAP_FIXED), region->f,  region->offset);
        } else {
            if ((error = scale_mmap_region(region, new_length)) < 0)
                return (void *)error;
            //uvm_switch(myproc()->pagetable);
            return region->addr;
        }
    }
    // 2: その場では拡大できない場合
    if (!(flags & MREMAP_MAYMOVE)) return (void *)error;

    if (MREMAP_FIXED)
        mapped_addr = (void *)mmap(new_addr, new_length, region->prot, (region->flags | MAP_FIXED), region->f, region->offset);
    else
        mapped_addr = (void *)mmap(new_addr, new_length, region->prot, (region->flags & ~MAP_FIXED), region->f, region->offset);

    if (IS_ERR(mapped_addr)) {
        return mapped_addr;

    }

    memmove(new_addr, region->addr, region->length);
    if ((error = msync(mapped_addr, new_length, MS_SYNC)) < 0)
        return (void *)error;

    if ((error = munmap(region->addr, region->length)) < 0)
        return (void *)error;
    //uvm_switch(myproc()->pagetable);
    //print_mmap_list(myproc(), "mremap");
    return mapped_addr;
}

long mprotect(void *addr, size_t length, int prot)
{
    struct proc *p = myproc();
    struct mmap_region *region;
    char *addr1, *addr2, *addr3;
    uint64_t length1, length2, length3;
    uint64_t offset1, offset2, offset3;
    int old_prot;
    long error;

    uint64_t addrp = (uint64_t)addr;
    if (addrp <= p->sz) {
        for (; addrp < (uint64_t)addr + length; addrp += PGSIZE) {
            pte_t *pte = walk(p->pagetable, addrp, 0);
            if (pte == NULL) {
                error("addr: 0x%lx is not mapping", addrp);
                return -ENOMEM;
            }

            if ((*pte & PTE_RO) && (prot == PROT_WRITE)) {
                error("wrong prot: prot: 0x%x, *pte: 0x%x", prot, *pte & 0xff);
                return -EACCES;
            }

            if (prot == PROT_NONE) {
                *pte &= ~PTE_U;
            } else {
                if (prot == PROT_READ)
                    *pte |= PTE_R;
                if (prot == PROT_WRITE)
                    *pte |= PTE_W;
                if (prot == PROT_EXEC)
                    *pte |= PTE_X;
                *pte |= PTE_U;
            }
        }

        return 0;
    }

    if (!is_mmap_region(p, addr, PGROUNDUP(length))) {
        error("invalid region: addr: %p, length: 0x%lx", addr, length);
        return -ENOMEM;
    }

    region = find_mmap_region(p, addr);

    if ((region->prot & (PROT_READ | PROT_WRITE)) == PROT_READ && (prot & PROT_WRITE)) {
        error("wrong prot: prot: 0x%x, regon->prot: 0x%x", prot, region->prot);
        return -EACCES;
    }

    length = PGROUNDUP(length);
    if (region->addr == addr && region->length == length && region->prot == prot)
        return 0;

    old_prot = region->prot;

    if (region->addr == addr && region->length == length) {
        region->prot = prot;
    } else if (region->addr == addr) {
        addr1 = region->addr;
        length1 = length;
        offset1 = region->offset;
        addr2 = addr1 + length1;
        length2 = region->length - length1;
        offset2 = region->f ? offset1 + length1 : 0;
        if ((error = munmap(region->addr, region->length)) < 0)
            return error;
        mmap(addr1, length1, prot, (region->flags | MAP_FIXED), region->f,  offset1);
        mmap(addr2, length2, old_prot, (region->flags | MAP_FIXED), region->f,  offset2);
    } else if ((addr + length) == (region->addr + region->length)) {
        addr1 = region->addr;
        length1 = region->length - length;
        offset1 = region->offset;
        addr2 = addr;
        length2 = length;
        offset2 = region->f ? offset1 + length1 : 0;
        if ((error = munmap(region->addr, region->length)) < 0)
            return error;
        mmap(addr1, length1, old_prot, (region->flags | MAP_FIXED), region->f,  offset1);
        mmap(addr2, length2, prot, (region->flags | MAP_FIXED), region->f,  offset2);
    } else {
        addr1 = region->addr;
        length1 = (uint64_t)(addr) - (uint64_t)addr1;
        offset1 = region->offset;
        addr2 = addr;
        length2 = length;
        offset2 = region->f ? offset1 + length1 : 0;
        addr3 = addr + length;
        length3 = region->length - length1 - length2;
        offset3 = region->f ? offset2 + length2 : 0;
        if ((error = munmap(region->addr, region->length)) < 0)
            return error;
        mmap(addr1, length1, old_prot, (region->flags | MAP_FIXED), region->f,  offset1);
        mmap(addr2, length2, prot, (region->flags | MAP_FIXED), region->f,  offset2);
        mmap(addr3, length3, old_prot, (region->flags | MAP_FIXED), region->f,  offset3);
    }

    if (p->pid == 7)
        print_mmap_list(p, "mprotect");

    return 0;
}

long msync(void *addr, size_t length, int flags)
{
    struct proc *p = myproc();
    long error = -EINVAL;

    if (!(flags & MS_ASYNC) && !(flags & MS_SYNC)) {
        flags |= MS_ASYNC;
    }

    if (flags & MS_ASYNC) {
        // Since  Linux  2.6.19, MS_ASYNC  is  in  fact  a no-op (from man(2))
        return 0;
    }

    // addrはページ境界になければならない。
    if (NOT_PAGEALIGN((uint64_t)addr)) return error;

    struct mmap_region *region = find_mmap_region(p, addr);
    if (region == NULL) {
        warn("no region with addr %p", addr);
        return 0;
    }

    if (region->length < length) {
        warn("length 0x%lx is bigger than region->length 0x%lx", length, region->length);
        length = region->length;
    }


    if ((region->flags & MAP_ANONYMOUS) == 0 && region->flags & MAP_SHARED && region->prot & PROT_WRITE) {
        for (uint64_t ra = (uint64_t)addr; ra < (uint64_t)addr + length; ra += PGSIZE) {
            pte_t *pte = walk(p->pagetable, ra, 0);
            if (!pte) panic("no pte");
            uint64_t flags = PTE_FLAGS(*pte);
            off_t offset = region->offset + ra - (uint64_t)region->addr;
            if (flags & PTE_D) {
                if (writeback(region->f, offset, ra) < 0) {
                    error("failed writeback");
                    return -EACCES;
                }
            }
            if (region->f->ip->size <= offset + PGSIZE)
                break;
        }
    }

    return 0;
}
