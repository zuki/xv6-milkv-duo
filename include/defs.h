#ifndef INC_DEFS_H
#define INC_DEFS_H

#include <common/riscv.h>
#include <linux/time.h>

struct buf;
struct context;
struct file;
struct inode;
struct dirent;
struct page;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
struct emmc;
struct slab_cache;
struct rusage;
struct k_sigaction;
struct pollfd;
struct tm;
struct mmap_region;
struct siginfo;

#define _cleanup_(x) __attribute__((cleanup(x)))

// bio.c
void            binit(void);
struct buf*     bread(uint32_t, uint32_t);
void            brelse(struct buf*);
void            bwrite(struct buf*);
void            bpin(struct buf*);
void            bunpin(struct buf*);

// buddy.c
void            buddy_init(void);
struct page *   buddy_alloc(size_t size);
void            buddy_free(struct page *page);

// clock.c
void            clockinit(void);
void            clockintr(void);
int             clock_gettime(int userout, clockid_t clk_id, struct timespec *tp);
int             clock_settime(clockid_t clk_id, const struct timespec *tp);
long            get_uptime(void);
long            get_ticks(void);

// console.c
void            consoleinit(void);
void            consoleintr(int);
void            consputc(int);

// exec.c
int             execve(char *path, char *const argv[], char *const envp[], int argc, int envc);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file *f, uint64_t addr, int n, int user);
int             filestat(struct file*, uint64_t addr);
int             filewrite(struct file *f, uint64_t addr, int n, int user);
long            sendfile(struct file *out_f, struct file *in_f, off_t offsetp, size_t count);
int             writeback(struct file *f, off_t off, uint64_t addr);
int             fileioctl(struct file*, unsigned long, void *argp);
long            filelseek(struct file *f, off_t offset, int whence);
long            filelink(char *oldpath, int olddirfd, char *newpath, int newdirfd);
long            filesymlink(char *target, char *linkpath, int dirfd);
ssize_t         filereadlink(char *path, int dirfd, uint64_t buf, size_t bufsize);
long            fileunlink(char *path, int dirfd, int flags);
struct inode *  create(char *path, int dirfd, short type, short major, short minor, mode_t mode);
long            fileopen(char *path, int dirfd, int flags, mode_t mode);
long            filechmod(char *path, int dirfd, mode_t mode);
long            filechown(struct file *f, char *path, int dirfd, uid_t owner, gid_t group, int flags);
struct file *   fileget(char *path);
long            faccess(char *path, int dirfd, int mode, int flags);
long            filerename(char *oldpath, int olddirfd, char *newpath, int newdirfd, uint32_t flags);

// fs.c
void            fsinit(int);
int             dirlink(struct inode *dp, char *name, uint32_t inum, uint16_t type);
struct inode*   dirlookup(struct inode *dp, char *name, uint32_t *poff);
int             direntlookup(struct inode *dp, int inum, struct dirent *dep, size_t *ofp);
struct inode*   ialloc(uint32_t, short);
struct inode*   idup(struct inode*);
void            iinit();
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char *path, int dirfd);
struct inode*   nameiparent(char *path, char *name, int dirfd);
int             readi(struct inode *ip, int user_dst, uint64_t dst, uint32_t off, uint32_t n);
void            stati(struct inode*, struct stat*);
int             writei(struct inode *ip, int user_src, uint64_t src, uint32_t off, uint32_t n);
void            itrunc(struct inode*);
int             unlink(struct inode *dp, uint32_t off);
int             getdents64(struct file *f, uint64_t data, size_t size);
int             permission(struct inode *ip, int mask);

// ramdisk.c
void            ramdiskinit(void);
void            ramdiskintr(void);
void            ramdiskrw(struct buf*, int write);

// kalloc.c
void*           kalloc(void);
void            kfree(void *pa);
void            kinit(void);

// kmalloc.c
void            kmfree(void *ap);
void *          kmalloc(size_t nbytes);

// log.c
void            initlog(int, struct superblock*);
void            log_write(struct buf*);
void            begin_op(void);
void            end_op(void);

// mmap.c
long            mmap(void *addr, size_t length, int prot, int flags, struct file*, off_t offset);
long            munmap(void *addr, size_t len);
void           *mremap(void *old_addr, size_t old_length, size_t new_length, int flags, void *new_addr);
long            mprotect(void *addr, size_t length, int prot);
long            msync(void *addr, size_t length, int flags);
uint64_t        get_perm(int prot, int flags);
long            mmap_load_pages(void *addr, size_t length, int prot, int flags, struct file *f, off_t offset);
void            print_mmap_list(struct proc *p, const char *title);
void            free_mmap_list(struct proc *p);
long            copy_mmap_regions(struct proc *parent, struct proc *child);
struct mmap_region  *find_mmap_region(struct proc *p, void *start);
bool            is_mmap_region(struct proc *p, void *addr, uint64_t length);
long            alloc_mmap_page(struct proc *p, uint64_t addr, uint64_t scause);

// page.c
void            page_init(void);
void *          page_address(const struct page *page);
struct page *   page_find_by_address(void *address);
struct page *   page_find_head(const struct page *page);
void            page_cleanup(struct page **page);
void            page_refcnt_inc(void *pa);
void            page_refcnt_dec(void *pa);
uint32_t        page_refcnt_get(void *pa);

// pipe.c
int             pipealloc(struct file**, struct file**, int);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, uint64_t, int);
int             pipewrite(struct pipe*, uint64_t, int);

// printf.c
int             printf(const char*, ...);
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);
void            debug_bytes(char *title, char *buf, int size);

// proc.c
struct cpu *    mycpu(void);
struct proc *   myproc(void);
int             cpuid(void);
void            delayms(unsigned long n);
void            delayus(unsigned long n);
int             either_copyout(int user_dst, uint64_t dst, void *src, uint64_t len);
int             either_copyin(void *dst, int user_src, uint64_t src, uint64_t len);
void            exit(int);
int             fork(void);
struct cpu*     getmycpu(void);
uint64_t        get_timer(uint64_t start);
int             growproc(int);
void            kdelay(unsigned long n);
int             kill(pid_t pid, int sig);
int             killed(struct proc*);
struct cpu*     mycpu(void);
struct proc*    myproc();
void            procdump(void);
void            procinit(void);
void            proc_mapstacks(pagetable_t);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64_t);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setkilled(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait4(pid_t pid, uint64_t status, int options, uint64_t ru);
void            wakeup(void*);
void            yield(void);
void            check_pending_signal(void);
long            sigsuspend(sigset_t *mask);
long            sigaction(int sig, struct k_sigaction *act, uint64_t oldact);
long            sigpending(uint64_t pending);
long            sigprocmask(int how, sigset_t *set, uint64_t oldset);
long            sigreturn(void);
void            send_signal(struct proc *p, int sig, struct siginfo *si);
//void            flush_signal_handlers(struct proc *p);
long            ppoll(struct pollfd *fds, nfds_t nfds, struct timespec *timeout_ts, sigset_t *sigmask);
pid_t           getpgid(pid_t pid);
long            setpgid(pid_t pid, pid_t pgid);
void            flush_signal_handlers(struct proc *p);

// rtc.c
void            rtc_init(void);
void            set_second_clock(uint32_t t);
time_t          get_second_clock(void);
time_t          rtc_time(time_t *t);
int             rtc_time_to_tm(time_t time, struct tm *tm);
int             rtc_gettime(struct timespec *tp);
int             rtc_settime(const struct timespec *tp);
void            rtc_strftime(struct tm *tm);
void            rtc_now(void);

// sbi.c
#ifndef CONFIG_RISCV_M_MODE
void            sbiinit(void);
void sbi_set_timer(unsigned long stime_value);
#else
static inline void sbiinit(void) { }
#endif

// signal.c
int     sigemptyset(sigset_t *set);
int     sigfillset(sigset_t *set);
int     sigaddset(sigset_t *set, int signum);
int     sigdelset(sigset_t *set, int signum);
int     sigismember(const sigset_t *set, int signum);
int     sigisemptyset(const sigset_t *set);
int     signotset(sigset_t *dest, const sigset_t *src);
int     sigorset(sigset_t *dest, const sigset_t *left, const sigset_t *right);
int     sigandset(sigset_t *dest, const sigset_t *left, const sigset_t *right);
int     siginitset(sigset_t *dest, sigset_t *src);

// sigret_syscall.S
void execute_sigret_syscall_start(void);
void execute_sigret_syscall_end(void);

// swtch.S
void            swtch(struct context*, struct context*);

// slab.c
void            slab_cache_init(void);
struct slab_cache *slab_cache_create(const char *name, uint32_t size, uint32_t alignment);
void            slab_cache_destroy(struct slab_cache *cache);
void *          slab_cache_alloc(struct slab_cache *cache);
void            slab_cache_free(struct slab_cache *cache, void *obj);

// spinlock.c
void            acquire(struct spinlock*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            push_off(void);
void            pop_off(void);

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// string.c
int             memcmp(const void*, const void*, uint32_t);
void*           memmove(void*, const void*, uint32_t);
void*           memcpy(void *, const void *, uint32_t);
void*           memset(void*, int, uint32_t);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint32_t);
char*           strncpy(char*, const char*, int);

// syscall.c
//int             argaddr(int, uint64_t *);
int             argint(int n, int *ip);
int             argstr(int n, char *buf, int max);
int             argu64(int n, uint64_t *u64);
int             argptr(int n, char *pp, size_t size);
int             fetchstr(uint64_t addr, char *buf, int max);
int             fetchaddr(uint64_t addr, uint64_t *ip);
int             fdalloc(struct file *f, int from);
void            syscall(void);

// trap.c
void            trapinit(void);
void            trapinithart(void);
void            usertrapret(void);

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartputc(int);
void            uartputc_sync(int);
int             uartgetc(void);

// vm.c
void            kvminit(void);
void            kvminithart(void);
void            kvmmap(pagetable_t kpgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);
int             mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, uint64_t perm);
pagetable_t     uvmcreate(void);
void            uvmfirst(pagetable_t pagetable, uchar *src, uint32_t sz);
uint64_t        uvmalloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz, int xperm);
uint64_t        uvmdealloc(pagetable_t pagetable, uint64_t oldsz, uint64_t newsz);
int             uvmcopy(struct proc *old, struct proc *new);
void            uvmfree(pagetable_t pagetable, uint64_t sz);
void            uvmunmap(pagetable_t pagetable, uint64_t va, uint64_t npages, int do_free);
void            uvmclear(pagetable_t pagetable, uint64_t va);
pte_t *         walk(pagetable_t pagetable, uint64_t va, int alloc);
uint64_t        walkaddr(pagetable_t pagetable, uint64_t va);
int             copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len);
int             copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len);
int             copyinstr(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t max);
void            uvmdump(pagetable_t pagetable, pid_t pid, char *name);
int             alloc_cow_page(pagetable_t pagetable, uint64_t va);
// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int);

// emmc.c
void            emmc_clear_interrupt(void);
void            emmc_intr(struct emmc *self);
int             emmc_init(struct emmc *self);
size_t          emmc_read(struct emmc *self, void *buf, size_t cnt);
size_t          emmc_write(struct emmc *self, void *buf, size_t cnt);
uint64_t        emmc_seek(struct emmc *self, uint64_t off);

// sd.c
void            sd_init(void);
void            sd_intr(void);
void            sd_rw(struct buf *);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#endif
