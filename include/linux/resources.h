#ifndef INC_LINUX_RESOURCES_H
#define INC_LINUX_RESOURCES_H

#include <common/types.h>
#include <linux/time.h>

typedef unsigned long long rlim_t;

struct rlimit {
        rlim_t rlim_cur;
        rlim_t rlim_max;
};

struct rusage {
        struct timeval ru_utime;
        struct timeval ru_stime;
        /* linux extentions, but useful */
        long    ru_maxrss;
        long    ru_ixrss;
        long    ru_idrss;
        long    ru_isrss;
        long    ru_minflt;
        long    ru_majflt;
        long    ru_nswap;
        long    ru_inblock;
        long    ru_oublock;
        long    ru_msgsnd;
        long    ru_msgrcv;
        long    ru_nsignals;
        long    ru_nvcsw;
        long    ru_nivcsw;
        /* room for more... */
        long    __reserved[16];
};


#define RLIM_INFINITY (~0ULL)
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIM_SAVED_MAX RLIM_INFINITY

#define RLIMIT_CPU     0
#define RLIMIT_FSIZE   1
#define RLIMIT_DATA    2
#define RLIMIT_STACK   3
#define RLIMIT_CORE    4
#ifndef RLIMIT_RSS
#define RLIMIT_RSS     5
#define RLIMIT_NPROC   6
#define RLIMIT_NOFILE  7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS      9
#endif
#define RLIMIT_LOCKS   10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE    13
#define RLIMIT_RTPRIO  14
#define RLIMIT_RTTIME  15
#define RLIMIT_NLIMITS 16

#define RLIM_NLIMITS RLIMIT_NLIMITS

#endif
