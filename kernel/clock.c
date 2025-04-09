#include <defs.h>
#include <common/types.h>
#include <linux/time.h>
#include <linux/capability.h>
#include <proc.h>
#include <spinlock.h>
#include <errno.h>
#include <printf.h>

#define HZ        (100)                 // 10 ms = 1 / 25 MHz
#define TICK_USEC (10000UL)
#define TICK_NSEC (10000000UL)

uint64_t tick_usec = TICK_USEC;        /* USER_HZ period (usec) */
uint64_t tick_nsec = TICK_NSEC;        /* ACTHZ period (nsec) */

// システムタイム (ticks)
uint64_t jiffies = 0;
// 現在時刻 (wall_time)
struct timespec xtime  __attribute__ ((aligned (16)));
// 直近のwall time更新時のjiffies
uint64_t wall_jiffies = 0;

struct spinlock clocklock;

// 現在時の更新
static void update_wall_time(uint64_t ticks)
{
    do {
        ticks--;
        xtime.tv_nsec += TICK_NSEC;         // 1 tick = 10ms = 10 * 10^6
        if (xtime.tv_nsec >= 1000000000) {  // nsec部分が1秒を超えたら
            xtime.tv_nsec -= 1000000000;    // nsecから1秒引いて
            xtime.tv_sec++;                 // secに1秒足す
        }
    } while (ticks);
}

// 現在時の調整
static inline void update_times(void)
{
    uint64_t ticks;

    ticks = jiffies - wall_jiffies;
    if (ticks) {
        wall_jiffies += ticks;
        update_wall_time(ticks);
    }
}

void clockinit(void)
{
    initlock(&clocklock, "clock");
    if (rtc_gettime(&xtime) < 0) {
        xtime.tv_nsec = 0;
        xtime.tv_sec = 1744166080;
    }
    info("clockinit ok");
}

void clockintr(void)
{
    acquire(&clocklock);
    jiffies++;
    update_times();
    //run_timer_list();
    //clock_reset();
    wakeup(&jiffies);
    release(&clocklock);
}

int clock_gettime(int userout, clockid_t clk_id, struct timespec *tp_addr)
{
    struct timespec tp;
    struct proc *p = myproc();

    switch(clk_id) {
        default:
            return -EINVAL;
        case CLOCK_REALTIME:
            tp.tv_nsec = xtime.tv_nsec;
            tp.tv_sec = xtime.tv_sec;
            break;
        case CLOCK_PROCESS_CPUTIME_ID:
        /*
            uint64_t ptime = (p->stime + p->utime) * TICK_NSEC;
            tp.tv_nsec = ptime % 1000000000;
            tp.tv_sec  = ptime / 1000000000;
        */
            tp.tv_nsec = 0;
            tp.tv_sec  = 0;
            break;
    }
    trace("clk: %d, tv_sec: %lld, tv_nsec: %lld", clk_id, tp.tv_sec, tp.tv_nsec);
    if (userout) {
        if (copyout(p->pagetable, (uint64_t)tp_addr, (char *)&tp, sizeof(struct timespec)) < 0)
        return -EFAULT;
    } else {
        memcpy(tp_addr, &tp, sizeof(tp));
    }

    return 0;
}

int clock_settime(clockid_t clk_id, const struct timespec *tp)
{
    switch(clk_id) {
        default:
            return -EINVAL;
        case CLOCK_REALTIME:
            if (capable(CAP_SYS_TIME)) {
                set_second_clock(tp->tv_sec);
            } else {
               return -EPERM;
            }
            break;
    }
    return 0;
}

long get_uptime(void)
{
    return jiffies * HZ;
}

long get_ticks(void)
{
    return jiffies;
}
