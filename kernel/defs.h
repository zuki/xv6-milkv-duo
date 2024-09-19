#ifndef INC_DEFS_H
#define INC_DEFS_H

#include "riscv.h"

struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
struct emmc;

// bio.c
void            binit(void);
struct buf*     bread(uint32_t, uint32_t);
void            brelse(struct buf*);
void            bwrite(struct buf*);
void            bpin(struct buf*);
void            bunpin(struct buf*);

// console.c
void            consoleinit(void);
void            consoleintr(int);
void            consputc(int);

// exec.c
int             exec(char*, char**);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, uint64_t, int n);
int             filestat(struct file*, uint64_t addr);
int             filewrite(struct file*, uint64_t, int n);
int             fileioctl(struct file*, unsigned long, void *argp);

// fs.c
void            fsinit(int);
int             dirlink(struct inode*, char*, uint32_t);
struct inode*   dirlookup(struct inode*, char*, uint*);
struct inode*   ialloc(uint32_t, short);
struct inode*   idup(struct inode*);
void            iinit();
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, int, uint64_t, uint32_t, uint32_t);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, int, uint64_t, uint32_t, uint32_t);
void            itrunc(struct inode*);

// ramdisk.c
void            ramdiskinit(void);
void            ramdiskintr(void);
void            ramdiskrw(struct buf*, int write);

// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);

// log.c
void            initlog(int, struct superblock*);
void            log_write(struct buf*);
void            begin_op(void);
void            end_op(void);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, uint64_t, int);
int             pipewrite(struct pipe*, uint64_t, int);

// printf.c
int             printf(const char*, ...);
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);

// proc.c
int             cpuid(void);
void            exit(int);
int             fork(void);
int             growproc(int);
void            proc_mapstacks(pagetable_t);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64_t);
int             kill(int);
int             killed(struct proc*);
void            setkilled(struct proc*);
struct cpu*     mycpu(void);
struct cpu*     getmycpu(void);
struct proc*    myproc();
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(uint64_t);
void            wakeup(void*);
void            yield(void);
int             either_copyout(int user_dst, uint64_t dst, void *src, uint64_t len);
int             either_copyin(void *dst, int user_src, uint64_t src, uint64_t len);
void            procdump(void);
void            kdelay(unsigned long n);
void            delayms(unsigned long n);
void            delayus(unsigned long n);
// sbi.c
#ifndef CONFIG_RISCV_M_MODE
void            sbiinit(void);
void sbi_set_timer(unsigned long stime_value);
#else
static inline void sbiinit(void) { }
#endif

// swtch.S
void            swtch(struct context*, struct context*);

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
void*           memset(void*, int, uint32_t);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint32_t);
char*           strncpy(char*, const char*, int);

// syscall.c
void            argint(int, int*);
int             argstr(int, char*, int);
void            argaddr(int, uint64_t *);
int             fetchstr(uint64_t, char*, int);
int             fetchaddr(uint64_t, uint64_t*);
void            syscall();

// trap.c
extern uint32_t     ticks;
void            trapinit(void);
void            trapinithart(void);
extern struct spinlock tickslock;
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
void            kvmmap(pagetable_t, uint64_t, uint64_t, uint64_t, uint64_t);
int             mappages(pagetable_t, uint64_t, uint64_t, uint64_t, uint64_t);
pagetable_t     uvmcreate(void);
void            uvmfirst(pagetable_t, uchar *, uint32_t);
uint64_t          uvmalloc(pagetable_t, uint64_t, uint64_t, int);
uint64_t          uvmdealloc(pagetable_t, uint64_t, uint64_t);
int             uvmcopy(pagetable_t, pagetable_t, uint64_t);
void            uvmfree(pagetable_t, uint64_t);
void            uvmunmap(pagetable_t, uint64_t, uint64_t, int);
void            uvmclear(pagetable_t, uint64_t);
pte_t *         walk(pagetable_t, uint64_t, int);
uint64_t          walkaddr(pagetable_t, uint64_t);
int             copyout(pagetable_t, uint64_t, char *, uint64_t);
int             copyin(pagetable_t, char *, uint64_t, uint64_t);
int             copyinstr(pagetable_t, char *, uint64_t, uint64_t);

// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int);

// virtio_disk.c
void            virtio_disk_init(void);
void            virtio_disk_rw(struct buf *, int);
void            virtio_disk_intr(void);

// gpio.c
void            gpioinit(void);

// pwm.c
void            pwminit(void);

// adc.c
void            adcinit(void);

// i2c.c
void            i2cinit(void);

// spi.c
void            spiinit(void);

// emmc.c

void emmc_clear_interrupt();
void emmc_intr(struct emmc *self);
int emmc_init(struct emmc *self, void (*sleep_fn)(void *), void *sleep_arg);
size_t emmc_read(struct emmc *self, void *buf, size_t cnt);
size_t emmc_write(struct emmc *self, void *buf, size_t cnt);
uint64_t emmc_seek(struct emmc *self, uint64_t off);

// sd.c
void sd_init(void);
void sd_intr(void);
void sd_rw(struct buf *);

// sdhci.c
int sdhci_init(void);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#endif
