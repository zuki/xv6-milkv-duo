#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include <defs.h>
#include <spinlock.h>
#include <proc.h>
#include <printf.h>
#include <errno.h>
#include <linux/syscall.h>
#include <linux/utsname.h>
#include <linux/time.h>
#include <debug.h>

// 指定されたアドレスがカレントプロセスのユーザアドレス範囲にあるかチェックする
static int in_user(uint64_t addr, size_t n)
{
    struct proc *p = myproc();

    // 1. addr + n が code+data 内にある
    if (addr + n <= p->sz)
        return 1;

    // 2. addr + n が mmap_region内にある
    if (is_mmap_region(p, (void *)addr, n))
        return 1;

    if (addr >= STACKBASE && addr < STACKTOP)
        return 1;

    return 0;
}

// カレントプロセスのaddrにあるuint64_tを取得する
// TODO: argptrに置き換え
int fetchaddr(uint64_t addr, uint64_t *ip)
{
    struct proc *p = myproc();
    if ((addr >= p->sz || addr + sizeof(uint64_t) > p->sz)          // コード領域外
    && (is_mmap_region(p, (void *)addr, sizeof(uint64_t)) == 0      // mmap領域外
    && (addr < STACKBASE || addr >= STACKTOP))) {                   // スタック領域外
        debug("addr; 0x%lx, p->sz: 0x%lx", addr, p->sz);
        return -1;
    }

    if (copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
        return -1;
    return 0;
}

// カレントプロセスのaddrにあるNULL終端の文字列を取得する
// nullを含まない文字列の長さを返す。maxになったらエラー。
// エラーの場合は-1を返す.
int fetchstr(uint64_t addr, char *buf, int max)
{
    struct proc *p = myproc();
    if (copyinstr(p->pagetable, buf, addr, max) < 0) {
        error("failed");
        return -1;
    }
    trace("buf: %s", buf);
    return strlen(buf);
}

// n番目の引数のraw値を返す
static uint64_t argraw(int n)
{
    struct proc *p = myproc();
    switch (n) {
    case 0:
        return p->trapframe->a0;
    case 1:
        return p->trapframe->a1;
    case 2:
        return p->trapframe->a2;
    case 3:
        return p->trapframe->a3;
    case 4:
        return p->trapframe->a4;
    case 5:
        return p->trapframe->a5;
    }
    panic("argraw");
    return -1;
}

int argu64(int n, uint64_t *u64)
{
    //debug("n: %d, u64: %p", n, u64);
    if (n > 5) {
        warn("too many system call parameters");
        return -1;
    }
    *u64 = argraw(n);
    return 0;
}

// n番目のシステムコール引数を取得する
int argint(int n, int *ip)
{
    if (n > 5) {
        warn("too many system call parameters");
        return -1;
    }
    *ip = argraw(n);
    return 0;
}

// n番目の引数をsizeバイトのブロックへのユーザ空間のアドレスとして
// カーネル空間のppにcopinする
// そのポインタがカレントプロセスのユーザ空間にあるかチェックする
//  *p = 0 の場合は成功とする
int argptr(int n, char *pp, size_t size)
{
    uint64_t addr;

    if (argu64(n, &addr) < 0) {
        return -1;
    }

    trace("n: %d, pp: %p, addr: 0x%lx, size: %ld", n, pp, addr, size);
    if (addr == 0) {
        pp = (char *)0;
        return 0;
    }

    if (in_user(addr, size)) {
        if (copyin(myproc()->pagetable, pp, addr, size) < 0)
            return -1;
        return 0;
    } else {
        return -1;
    }
}

// 引数をアドレス値として取り出す.
// 値の正当性はcopyin/copyoutで行われるのでここではチェックしない
#if 0
void argaddr(int n, uint64_t *ip)
{
    *ip = argraw(n);
}
#endif

// n番目のシステムコール引数をnull終端の文字列として取得する.
// 最大max文字までbufにコピーする。
// 成功の場合、（NULLを含む）文字列長を返す。エラーの場合は-1を返す。
int argstr(int n, char *buf, int max)
{
    uint64_t addr;
    if (argu64(n, &addr) < 0)
        return -1;
    return fetchstr(addr, buf, max);
}

typedef long (*func)();

// Prototypes for the functions that handle system calls.
extern long sys_llistxattr(void);
extern char *sys_getcwd(void);
extern long sys_clone(void);
extern long sys_exit(void);
extern long sys_exit_group(void);
extern long sys_fcntl(void);
extern long sys_wait4(void);
extern long sys_pipe2(void);
extern long sys_read(void);
extern long sys_readv(void);
extern long sys_kill(void);
extern long sys_execve(void);
extern long sys_fstat(void);
extern long sys_utimensat(void);
extern long sys_fstatat(void);
extern long sys_chdir(void);
extern long sys_fchmodat(void);
extern long sys_fchownat(void);
extern long sys_fchown(void);
extern long sys_dup(void);
extern long sys_dup3(void);
extern long sys_getpid(void);
extern long sys_getppid(void);
extern long sys_gettid(void);
extern long sys_setgid(void);
extern long sys_setreuid(void);
extern long sys_setuid(void);
extern long sys_setresuid(void);
extern long sys_getresuid(void);
extern long sys_setresgid(void);
extern long sys_getresgid(void);
extern long sys_setfsuid(void);
extern long sys_setfsgid(void);
extern long sys_getuid(void);
extern long sys_geteuid(void);
extern long sys_getgid(void);
extern long sys_getegid(void);
extern long sys_getdents64(void);
extern long sys_lseek(void);
extern long sys_brk(void);
extern long sys_nanosleep(void);
extern long sys_uptime(void);
extern long sys_openat(void);
extern long sys_ppoll(void);
extern long sys_write(void);
extern long sys_writev(void);
extern long sys_sendfile(void);
extern long sys_mknodat(void);
extern long sys_unlinkat(void);
extern long sys_linkat(void);
extern long sys_symlinkat(void);
extern ssize_t sys_readlinkat(void);
extern long sys_mkdirat(void);
extern long sys_close(void);
extern long sys_ioctl(void);
extern long sys_mmap(void);
extern long sys_munmap(void);
//extern long sys_mremap(void);
extern long sys_mprotect(void);
extern long sys_msync(void);
extern long sys_fadvise64(void);
extern long sys_rt_sigsuspend(void);
extern long sys_rt_sigaction(void);
extern long sys_rt_sigprocmask(void);
extern long sys_rt_sigpending(void);
extern long sys_rt_sigreturn(void);
extern long sys_sysinfo(void);
extern long sys_set_tid_address(void);
extern long sys_setpgid(void);
extern long sys_getpgid(void);
extern long sys_prlimit64(void);
extern long sys_renameat2(void);
extern mode_t sys_umask(void);
extern long sys_sched_getaffinity(void);
extern long sys_faccessat2(void);

long sys_clock_gettime()
{
    clockid_t clk_id;
    uint64_t tp;

    if (argint(0, (clockid_t *)&clk_id) < 0 || argu64(1, &tp) < 0)
        return -EINVAL;

    trace("clk_id: %d, tp: 0x%p\n", clk_id, tp);

    return clock_gettime(1, clk_id, (struct timespec *)tp);
}

long sys_clock_settime()
{
    clockid_t clk_id;
    struct timespec tp;

    if (argint(0, (clockid_t *)&clk_id) < 0 || argptr(1, (char *)&tp, sizeof(struct timespec)) < 0)
        return -EINVAL;

    trace("clk_id: %d, tp.sec: %ld\n", clk_id, tp.tv_sec);

    return clock_settime(clk_id, &tp);
}


long sys_futex(void) {
    uint64_t uaddr, uaddr2;
    int op, val, val3;
    struct timespec timeout;
    static int count = 0;

    if (argu64(0, &uaddr) < 0 || argint(1, &op) < 0
     || argint(2, &val) < 0 || argptr(3, (char *)&timeout, sizeof(struct timespec)) < 0
     || argu64(4, &uaddr2) < 0 || argint(5, &val3) < 0) {
        return -EINVAL;
    }

    if (++count < 5) {
        trace("uaddr: 0x%lx, op: %d, val: %d, timeout: %p, uaddr2: 0x%lx, val3: %d", uaddr, op, val, &timeout, uaddr2, val3);
    }
    if (op & 128)
        return -ENOSYS;
    else
        return 0;
}

long sys_uname(void) {
    uint64_t utsp;
    struct utsname uts;

    if (argu64(0, &utsp) < 0)
        return -EINVAL;

    strncpy(uts.sysname, UNAME_SYSNAME, sizeof(UNAME_SYSNAME));
    strncpy(uts.nodename, UNAME_NODENAME, sizeof(UNAME_NODENAME));
    strncpy(uts.release, UNAME_RELEASE, sizeof(UNAME_RELEASE));
    strncpy(uts.version, UNAME_VERSION, sizeof(UNAME_VERSION));
    strncpy(uts.machine, UNAME_MACHINE, sizeof(UNAME_MACHINE));
    copyout(myproc()->pagetable, utsp, (char *)&uts, sizeof(struct utsname));

    return 0;
}

long sys_debug(void) {
    char name[MAXPATH];
    uint64_t val;

    if (argstr(0, name, MAXPATH) < 0 || argu64(1, &val) < 0)
        return -EINVAL;
    debug("%s: 0x%lx", name, val);
    return 0;
}

long sys_dso(void) {
    char name[MAXPATH];
    uint64_t dsop;
    struct dso dso;

    if (argstr(0, name, MAXPATH) < 0 || argu64(1, &dsop) < 0)
        return -EINVAL;

    copyin(myproc()->pagetable, (char *)&dso, dsop, sizeof(struct dso));

    debug_bytes(name, (char *)&dso, sizeof(struct dso));
    return 0;
}

long sys_libc(void) {
    int stage;
    uint64_t libcp;
    struct __libc libc;

    if (argint(0, &stage) < 0 || argu64(1, &libcp) < 0)
        return -EINVAL;

    copyin(myproc()->pagetable, (char *)&libc, libcp, sizeof(struct __libc));
    debug("LIBC [%d] (0x%lx): %x-%x-%x-%x-%x, auxv: %p, head: %p, size: 0x%lx, align: 0x%lx, cnt: 0x%lx, page: 0x%lx, locale: %p",
        stage, libcp, libc.can_do_threads, libc.threaded, libc.secure, libc.need_locks,
        libc.threads_minus_1, libc.auxv, libc.tls_head, libc.tls_size,
        libc.tls_align, libc.tls_cnt, libc.page_size, libc.global_locale);
    return 0;
}

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static func syscalls[] = {
    [SYS_llistxattr] = sys_llistxattr,          //  12
    [SYS_getcwd]    = (func)sys_getcwd,         //  17
    [SYS_dup]       = sys_dup,                  //  23
    [SYS_dup3]      = sys_dup3,                 // 24
    [SYS_fcntl]     = sys_fcntl,                //  25
    [SYS_ioctl]     = sys_ioctl,                //  29
    [SYS_mknodat]   = sys_mknodat,              //  33
    [SYS_mkdirat]   = sys_mkdirat,              //  34
    [SYS_unlinkat]  = sys_unlinkat,             //  35
    [SYS_symlinkat] = sys_symlinkat,            //  36
    [SYS_linkat]    = sys_linkat,               //  37
    [SYS_chdir]     = sys_chdir,                //  49
    [SYS_fchmodat]  = sys_fchmodat,             //  53
    [SYS_fchownat]  = sys_fchownat,             //  54
    [SYS_fchown]    = sys_fchown,               //  55
    [SYS_openat]    = sys_openat,               //  56
    [SYS_close]     = sys_close,                //  57
    [SYS_pipe2]     = sys_pipe2,                //  59
    [SYS_getdents64] = sys_getdents64,          //  61
    [SYS_lseek]     = sys_lseek,                //  62
    [SYS_read]      = sys_read,                 //  63
    [SYS_write]     = sys_write,                //  64
    [SYS_readv]     = sys_readv,                //  65
    [SYS_writev]    = sys_writev,               //  66
    [SYS_sendfile]  = sys_sendfile,             //  71
    [SYS_ppoll]     = sys_ppoll,                //  73
    [SYS_readlinkat] = (func)sys_readlinkat,    //  78
    [SYS_newfstatat] = sys_fstatat,             //  79
    [SYS_fstat]     = sys_fstat,                //  80
    [SYS_utimensat] = sys_utimensat,            //  88
    [SYS_exit]      = sys_exit,                 //  93
    [SYS_exit_group] = sys_exit_group,          //  94
    [SYS_set_tid_address] = sys_set_tid_address,    //  96
    [SYS_futex]     = sys_futex,                //  98
    [SYS_nanosleep] = sys_nanosleep,            // 101
    [SYS_clock_settime] = sys_clock_settime,    // 112
    [SYS_clock_gettime] = sys_clock_gettime,    // 113
    [SYS_sched_getaffinity] = sys_sched_getaffinity, // 123
    [SYS_kill]      = sys_kill,                 // 129
    [SYS_rt_sigsuspend] = sys_rt_sigsuspend,    // 133
    [SYS_rt_sigaction] = sys_rt_sigaction,      // 134
    [SYS_rt_sigprocmask] = sys_rt_sigprocmask,  // 135
    [SYS_rt_sigpending] = sys_rt_sigpending,    // 136
    [SYS_rt_sigreturn] = sys_rt_sigreturn,      // 139
    [SYS_setgid]    = sys_setgid,               // 144
    [SYS_setreuid]  = sys_setreuid,             // 145
    [SYS_setuid]    = sys_setuid,               // 146
    [SYS_setresuid] = sys_setresuid,            // 147
    [SYS_getresuid] = sys_getresuid,            // 148
    [SYS_setresgid] = sys_setresgid,            // 149
    [SYS_getresgid] = sys_getresgid,            // 150
    [SYS_setfsuid]  = sys_setfsuid,             // 151
    [SYS_setfsgid]  = sys_setfsgid,             // 152
    [SYS_setpgid]   = sys_setpgid,              // 154
    [SYS_getpgid]   = sys_getpgid,              // 155
    [SYS_uname]     = sys_uname,                // 160
    [SYS_umask]     = (func)sys_umask,          // 166
    [SYS_getpid]    = sys_getpid,               // 172
    [SYS_getppid]   = sys_getppid,              // 173
    [SYS_setuid]    = sys_setuid,               // 146
    [SYS_getuid]    = sys_getuid,               // 174
    [SYS_geteuid]   = sys_geteuid,              // 175
    [SYS_getgid]    = sys_getgid,               // 176
    [SYS_getegid]   = sys_getegid,              // 177
    [SYS_gettid]    = sys_gettid,               // 178
    [SYS_gettid]    = sys_gettid,               // 178
    [SYS_sysinfo]   = sys_sysinfo,              // 179
    [SYS_brk]       = sys_brk,                  // 214
    [SYS_munmap]    = sys_munmap,               // 215
//    [SYS_mremap]    = sys_mremap,               // 216
    [SYS_clone]     = sys_clone,                // 220
    [SYS_execve]    = sys_execve,               // 221
    [SYS_mmap]      = sys_mmap,                 // 222
    [SYS_fadvise64] = sys_fadvise64,            // 223
    [SYS_mprotect]  = sys_mprotect,             // 226
    [SYS_msync]     = sys_msync,                // 227
    [SYS_wait4]     = sys_wait4,                // 260
    [SYS_prlimit64] = sys_prlimit64,            // 261
    [SYS_renameat2] = sys_renameat2,            // 276
    [SYS_faccessat2] = sys_faccessat2,          // 439
    [SYS_dso]       = sys_dso,                  // 997
    [SYS_libc]      = sys_libc,                 // 998
    [SYS_debug]     = sys_debug,                // 999
};

__attribute__((unused)) static char *syscall_names[] = {
    [SYS_llistxattr] = "sys_llistxattr",          // 12
    [SYS_getcwd] = "sys_getcwd",                  // 17
    [SYS_dup] = "sys_dup",                        // 23
    [SYS_dup3] = "sys_dup3",                      // 24
    [SYS_fcntl] = "sys_fcntl",                    // 25
    [SYS_ioctl] = "sys_ioctl",                    // 29
    [SYS_mknodat] = "sys_mknodat",                // 33
    [SYS_mkdirat] = "sys_mkdirat",                // 34
    [SYS_unlinkat] = "sys_unlinkat",              // 35
    [SYS_symlinkat] = "sys_symlinkat",            // 36
    [SYS_linkat] = "sys_linkat",                  // 37
    [SYS_renameat] = "sys_renameat",              // 38
    [SYS_umount2] = "sys_umount2",                // 39
    [SYS_mount] = "sys_mount",                    // 40
    [SYS_faccessat] = "sys_faccessat",            // 48
    [SYS_chdir] = "sys_chdir",                    // 49
    [SYS_fchmodat] = "sys_fchmodat",              // 53
    [SYS_fchownat] = "sys_fchownat",              // 54
    [SYS_fchown] = "sys_fchown",                  // 55
    [SYS_openat] = "sys_openat",                  // 56
    [SYS_close] = "sys_close",                    // 57
    [SYS_pipe2] = "sys_pipe2",                    // 59
    [SYS_getdents64] = "sys_getdents64",          // 61
    [SYS_lseek] = "sys_lseek",                    // 62
    [SYS_read] = "sys_read",                      // 63
    [SYS_write] = "sys_write",                    // 64
    [SYS_readv] = "sys_readv",                    // 65
    [SYS_writev] = "sys_writev",                  // 66
    [SYS_pread64] = "sys_pread64",                // 67
    [SYS_sendfile] = "sys_sendfile",              // 71
    [SYS_ppoll] = "sys_ppoll",                    // 73
    [SYS_readlinkat] = "sys_readlinkat",          // 78
    [SYS_newfstatat] = "sys_fstatat",             // 79
    [SYS_fstat] = "sys_fstat",                    // 80
    [SYS_fstat] = "sys_fstat",                    // 80
    [SYS_fsync] = "sys_fsync",                    // 82
    [SYS_fdatasync] = "sys_fdatasync",            // 83
    [SYS_utimensat] = "sys_utimensat",            // 88
    [SYS_exit] = "sys_exit",                      // 93
    [SYS_exit_group] = "sys_exit_group",          // 94
    [SYS_set_tid_address] = "sys_set_tid_address", // 96
    [SYS_futex] = "SYS_futex",                    // 98
    [SYS_nanosleep] = "sys_nanosleep",            // 101
    [SYS_getitimer] = "sys_getitimer",            // 102
    [SYS_setitimer] = "sys_setitimer",            // 103
    [SYS_clock_settime] = "sys_clock_settime",    // 112
    [SYS_clock_gettime] = "sys_clock_gettime",    // 113
    [SYS_sched_getaffinity] = "sys_sched_getaffinity", // 123
    [SYS_sched_yield] = "sys_yield",              // 124
    [SYS_kill] = "sys_kill",                      // 129
    [SYS_tkill] = "sys_tkill",                    // 130
    [SYS_rt_sigsuspend] = "sys_rt_sigsuspend",    // 133
    [SYS_rt_sigaction] = "sys_rt_sigaction",      // 134
    [SYS_rt_sigprocmask] = "sys_rt_sigprocmask",  // 135
    [SYS_rt_sigpending] = "sys_rt_sigpending",    // 136
    [SYS_rt_sigreturn] = "sys_rt_sigreturn",      // 139
    [SYS_setregid] = "sys_setregid",              // 143
    [SYS_setgid] = "sys_setgid",                  // 144
    [SYS_setreuid] = "sys_setreuid",              // 145
    [SYS_setuid] = "sys_setuid",                  // 146
    [SYS_setresuid] = "sys_setresuid",            // 147
    [SYS_getresuid] = "sys_getresuid",            // 148
    [SYS_setresgid] = "sys_setresgid",            // 149
    [SYS_getresgid] = "sys_getresgid",            // 150
    [SYS_setfsuid] = "sys_setfsuid",              // 151
    [SYS_setfsgid] = "sys_setfsgid",              // 152
    [SYS_setpgid] = "sys_setpgid",                // 154
    [SYS_getpgid] = "sys_getpgid",                // 155
    [SYS_getgroups] = "sys_getgroups",            // 158
    [SYS_setgroups] = "sys_setgroups",            // 159
    [SYS_uname] = "sys_uname",                    // 160
    [SYS_umask] = "sys_umask",                    // 166
    [SYS_getpid] = "sys_getpid",                  // 172
    [SYS_getppid] = "sys_getppid",                // 173
    [SYS_getuid] = "sys_getuid",                  // 174
    [SYS_geteuid] = "sys_geteuid",                // 175
    [SYS_getgid] = "sys_getgid",                  // 176
    [SYS_getegid] = "sys_getegid",                // 177
    [SYS_gettid] = "sys_gettid",                  // 178
    [SYS_sysinfo] = "sys_sysinfo",                // 179
    [SYS_brk] = "sys_brk",                        // 214
    [SYS_munmap] = "sys_munmap",                  // 215
    [SYS_mremap] = "sys_mremap",                  // 216
    [SYS_clone] = "sys_clone",                    // 220
    [SYS_execve] = "sys_execve",                  // 221
    [SYS_mmap] = "sys_mmap",                      // 222
    [SYS_fadvise64] = "sys_fadvise64",            // 223
    [SYS_mprotect] = "sys_mprotect",              // 226
    [SYS_msync] = "sys_msync",                    // 227
    [SYS_madvise] = "sys_madvise",                // 233
    [SYS_wait4] = "sys_wait4",                    // 260
    [SYS_prlimit64] = "sys_prlimit64",            // 261
    [SYS_renameat2] = "sys_renameat2",            // 276
    [SYS_getrandom] = "sys_getrandom",            // 278
    [SYS_faccessat2] = "sys_faccessat2",          // 439
    [SYS_dso]       = "sys_dso",                  // 997
    [SYS_libc]      = "sys_libc",                 // 998
    [SYS_debug]     = "sys_debug",                // 999
};

// 引数の個数
__attribute__((unused)) static int syscall_params[] = {
    [SYS_llistxattr] = 3,                       // 12
    [SYS_getcwd] = 2,                           // 17
    [SYS_dup] = 1,                              // 23
    [SYS_dup3] = 3,                             // 24
    [SYS_fcntl] = 2,                            // 25
    [SYS_ioctl] = 2,                            // 29
    [SYS_mknodat] = 4,                          // 33
    [SYS_mkdirat] = 3,                          // 34
    [SYS_unlinkat] = 3,                         // 35
    [SYS_symlinkat] = 3,                        // 36
    [SYS_linkat] = 5,                           // 37
    [SYS_renameat] = 4,                         // 38
    [SYS_umount2] = 2,                          // 39
    [SYS_mount] = 5,                            // 40
    [SYS_faccessat] = 4,                        // 48
    [SYS_chdir] = 1,                            // 49
    [SYS_fchmodat] = 4,                         // 53
    [SYS_fchownat] = 5,                         // 54
    [SYS_fchown] = 3,                           // 55
    [SYS_openat] = 4,                           // 56
    [SYS_close] = 1,                            // 57
    [SYS_pipe2] = 2,                            // 59
    [SYS_getdents64] = 3,                       // 61
    [SYS_lseek] = 3,                            // 62
    [SYS_read] = 3,                             // 63
    [SYS_write] = 3,                            // 64
    [SYS_readv] = 3,                            // 65
    [SYS_writev] = 3,                           // 66
    [SYS_pread64] = 4,                          // 67
    [SYS_sendfile] = 4,                         // 71
    [SYS_ppoll] = 4,                            // 73
    [SYS_readlinkat] = 4,                       // 78
    [SYS_newfstatat] = 4,                       // 79
    [SYS_fstat] = 2,                            // 80
    [SYS_fstat] = 0,                            // 81
    [SYS_fsync] = 1,                            // 82
    [SYS_fdatasync] = 1,                        // 83
    [SYS_utimensat] = 4,                        // 88
    [SYS_exit] = 1,                             // 93
    [SYS_exit_group] = 1,                       // 94
    [SYS_set_tid_address] = 1,                  // 96
    [SYS_futex] = 6,                            // 98
    [SYS_nanosleep] = 2,                        // 101
    [SYS_getitimer] = 2,                        // 102
    [SYS_setitimer] = 3,                        // 103
    [SYS_clock_settime] = 2,                    // 112
    [SYS_clock_gettime] = 2,                    // 113
    [SYS_sched_getaffinity] = 3,                // 123
    [SYS_sched_yield] = 0,                      // 124
    [SYS_kill] = 2,                             // 129
    [SYS_tkill] = 2,                            // 130
    [SYS_rt_sigsuspend] = 2,                    // 133
    [SYS_rt_sigaction] = 3,                     // 134
    [SYS_rt_sigprocmask] = 4,                   // 135
    [SYS_rt_sigpending] = 2,                    // 136
    [SYS_rt_sigreturn] = 0,                     // 139
    [SYS_setregid] = 2,                         // 143
    [SYS_setgid] = 1,                           // 144
    [SYS_setreuid] = 2,                         // 145
    [SYS_setuid] = 1,                           // 146
    [SYS_setresuid] = 3,                        // 147
    [SYS_getresuid] = 3,                        // 148
    [SYS_setresgid] = 3,                        // 149
    [SYS_getresgid] = 3,                        // 150
    [SYS_setfsuid] = 1,                         // 151
    [SYS_setfsgid] = 1,                         // 152
    [SYS_setpgid] = 2,                          // 154
    [SYS_getpgid] = 1,                          // 155
    [SYS_getgroups] = 2,                        // 158
    [SYS_setgroups] = 2,                        // 159
    [SYS_uname] = 1,                            // 160
    [SYS_umask] = 1,                            // 166
    [SYS_getpid] = 0,                           // 172
    [SYS_getppid] = 0,                          // 173
    [SYS_getuid] = 0,                           // 174
    [SYS_geteuid] = 0,                          // 175
    [SYS_getgid] = 0,                           // 176
    [SYS_getegid] = 0,                          // 177
    [SYS_gettid] = 0,                           // 178
    [SYS_sysinfo] = 1,                          // 179
    [SYS_brk] = 1,                              // 214
    [SYS_munmap] = 2,                           // 215
    [SYS_mremap] = 4,                           // 216
    [SYS_clone] = 5,                            // 220
    [SYS_execve] = 3,                           // 221
    [SYS_mmap] = 6,                             // 222
    [SYS_fadvise64] = 4,                        // 223
    [SYS_mprotect] = 3,                         // 226
    [SYS_msync] = 3,                            // 227
    [SYS_madvise] = 3,                          // 233
    [SYS_wait4] = 4,                            // 260
    [SYS_prlimit64] = 2,                        // 261
    [SYS_renameat2] = 5,                        // 276
    [SYS_getrandom] = 3,                        // 278
    [SYS_faccessat2] = 4,                       // 439
    [SYS_dso] = 2,                              // 997
    [SYS_libc] = 2,                             // 998
    [SYS_debug]     = 2,                        // 999
};


// システムコールを処理する
void syscall(void)
{
    int num;
    struct proc *p = myproc();
    long ret;

    // システムコール番号を取得する
    num = p->trapframe->a7;
    trace("tp: 0x%l016x", r_tp());
    // 該当の関数を実行する
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        // Use num to lookup the system call function for num, call it,
        // and store its return value in p->trapframe->a0
#if 0
        //if (p->pid == 4 && num != SYS_writev && num != SYS_read) {
        if (p->pid >= 3 && p->pid <= 6) {
            switch(syscall_params[num]) {
            case 5:
                debug("pid[%d] (%s) a0: 0x%lx, a1: 0x%lx, a2: 0x%lx, a3: 0x%lx, a4: 0x%lx", p->pid, syscall_names[num],
                p->trapframe->a0, p->trapframe->a1, p->trapframe->a2, p->trapframe->a3, p->trapframe->a4);
                break;
            case 4:
                debug("pid[%d] (%s) a0: 0x%lx, a1: 0x%lx, a2: 0x%lx, a3: 0x%lx", p->pid, syscall_names[num],
                p->trapframe->a0, p->trapframe->a1, p->trapframe->a2, p->trapframe->a3);
                break;
            case 3:
                debug("pid[%d] (%s) a0: 0x%lx, a1: 0x%lx, a2: 0x%lx", p->pid, syscall_names[num],
                p->trapframe->a0, p->trapframe->a1, p->trapframe->a2);
                break;
            case 2:
                debug("pid[%d] (%s) a0: 0x%lx, a1: 0x%lx", p->pid, syscall_names[num],
                p->trapframe->a0, p->trapframe->a1);
                break;
            case 1:
                debug("pid[%d] (%s) a0: 0x%lx", p->pid, syscall_names[num], p->trapframe->a0);
                break;
            case 0:
                debug("pid[%d] (%s)", p->pid, syscall_names[num]);
                break;
            default:
                debug("pid[%d] (%s) a0: 0x%lx, a1: 0x%lx, a2: 0x%lx, a3: 0x%lx, a4: 0x%lx, a5: 0x%lx, a6: 0x%lx", p->pid, syscall_names[num],
                p->trapframe->a0, p->trapframe->a1, p->trapframe->a2, p->trapframe->a3, p->trapframe->a4, p->trapframe->a5, p->trapframe->a6);
                break;
            }
        }
#endif
        ret = syscalls[num]();

#if 0
        if (num != SYS_writev && num != SYS_read) {
            debug("pid[%d] (%s) ret: 0x%lx", p->pid, syscall_names[num], ret);
        }
#endif

        p->trapframe->a0 = ret;
    } else {
        debug("[%d] %s: unknown sys call %d: %s\n",
                p->pid, p->name, num, syscall_names[num]);
        p->trapframe->a0 = -1;
    }
}
