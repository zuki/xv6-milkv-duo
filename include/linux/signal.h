#ifndef INC_LINUX_SIGNAL_H
#define INC_LINUX_SIGNAL_H

#include <common/types.h>

#define NSIG   32

typedef void (*sighandler_t)(int);

// Fake functions
#define SIG_ERR     ((sighandler_t) -1)     // error return
#define SIG_DFL     ((sighandler_t) 0)      // default signal handler
#define SIG_IGN     ((sighandler_t) 1)      // ignore signal

#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGIOT     SIGABRT
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED SIGSYS
#define _NSIG     65


#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

#define SI_ASYNCNL (-60)
#define SI_TKILL (-6)
#define SI_SIGIO (-5)
#define SI_ASYNCIO (-4)
#define SI_MESGQ (-3)
#define SI_TIMER (-2)
#define SI_QUEUE (-1)
#define SI_USER 0
#define SI_KERNEL 128

#define SA_NOCLDSTOP  1
#define SA_NOCLDWAIT  2
#define SA_SIGINFO    4
#define SA_ONSTACK    0x08000000
#define SA_RESTART    0x10000000
#define SA_NODEFER    0x40000000
#define SA_RESETHAND  0x80000000

struct sigaction {
    sighandler_t    sa_handler;
    sigset_t        sa_mask;
    int             sa_flags;
    void (*sa_restorer)(void);
};

struct k_sigaction {
    sighandler_t    handler;
    unsigned long   flags;
    unsigned        mask[2];
    void           *unused;
};


#endif
