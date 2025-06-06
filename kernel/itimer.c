/*
 * linux/kernel/itimer.c
 *
 * Copyright (C) 1992 Darren Senn
 */

/* These are all the functions necessary to implement itimers */
#include <types.h>
#include <errno.h>
#include <spinlock.h>
#include <proc.h>
#include <linux/signal.h>
#include <linux/time.h>

/*
 * 明らかなオーバフローを回避するためにtimevalをjiffiesに変更する。
 *
 * tv_*secの値は符号付きだが、itimersを行う際に本当に符号付きの値として
 * 使用すべきについては何も示されていないようだ。POSIXはこれについて言及
 * していない（しかし、alarm()がチェックなしでitimersを使うのであれば、
 * 符号なし演算を使わなければならない)。
 */
static uint64_t tvtojiffies(struct timeval *value)
{
    uint64_t sec =  (uint64_t) value->tv_sec;
    uint64_t usec = (uint64_t) value->tv_usec;

    if (sec > (ULONG_MAX / HZ))
        return ULONG_MAX;
    usec += 1000000 / HZ - 1;
    usec /= 1000000 / HZ;
    return HZ * sec + usec;
}

static void jiffiestotv(uint64_t jiffs, struct timeval *value)
{
    value->tv_usec = (jiffs % HZ) * (1000000 / HZ);
    value->tv_sec  = jiffs / HZ;
}

long getitimer(int which, uint64_t valuep)
{
    uint64_t val, interval;
    struct proc *p = myproc();
    struct itimerval value;

    switch (which) {
    case ITIMER_REAL:
        acquire(&p->time_lock);
        interval = p->it_real_incr;
        val = 0;
        if (timer_pending(&p->real_timer)) {
            val = p->real_timer.expires;
            if ((int64_t) val <= 0)
                val = 1;
        }
        release(&p->time_lock);
        break;
    case ITIMER_VIRTUAL:
#if 0
        val = p->it_virt_value;
        interval = p->it_virt_incr;
#endif
        return -EFAULT;
        break;
    case ITIMER_PROF:
#if 0
        val = p->it_prof_value;
        interval = p->it_prof_incr;
#endif
        return -EFAULT;
        break;
    default:
        return -EINVAL;
    }
    jiffiestotv(val, &value.it_value);
    jiffiestotv(interval, &value.it_interval);
    if (copyout(p->pagetable, valuep, (char *)&value, sizeof(struct itimerval)) <0) {
        error("failed copyout value");
        return -EFAULT;
    }
    return 0;
}

void it_real_fn(uint64_t __data)
{
    struct proc *p = (struct proc *) __data;
    uint64_t interval;

    kill(p->pid, SIGALRM);
    interval = p->it_real_incr;
    if (interval) {
        if (interval > (uint64_t) LONG_MAX)
            interval = LONG_MAX;
        p->real_timer.expires = interval;
        add_timer(&p->real_timer);
    }
}

long setitimer(int which, uint64_t new_valuep, uint64_t old_valuep)
{
    uint64_t interval, value;
    long error;
    struct proc *p = myproc();
    struct itimerval new_value;

    if (new_valuep && copyin(p->pagetable, (char *)&new_value, new_valuep, sizeof(struct itimerval)) < 0) {
        error("failed copyin new_value");
        return -EFAULT;
    } else {
        memset(&new_value, 0, sizeof(struct itimerval));
    }

    interval = tvtojiffies(&new_value.it_interval);
    value = tvtojiffies(&new_value.it_value);
    if (p->pid == 12)
        trace("jiffies: 0x%lx, val->interval: 0x%x, val->value: 0x%lx, interval: 0x%x, value: 0x%x",
            jiffies, new_value.it_interval.tv_sec, new_value.it_value.tv_sec, interval, value);

    // 現在のtimerをセット
    if (old_valuep && (error = getitimer(which, old_valuep)) < 0)
        return error;

    switch (which) {
        case ITIMER_REAL:
            del_timer_sync(&p->real_timer);
            p->it_real_value = value;
            p->it_real_incr = interval;
            if (!value)
                break;
            if (value > (uint64_t) LONG_MAX)
                value = LONG_MAX;
            interval = value;
            p->real_timer.expires = interval;
            add_timer(&p->real_timer);
            break;
        case ITIMER_VIRTUAL:
#if 0
            if (j)
                j++;
            p->it_virt_value = j;
            p->it_virt_incr = i;
#endif
            warn("ITIMER_VIRTUAL not implment");
            break;
        case ITIMER_PROF:
#if 0
            if (j)
                j++;
            p->it_prof_value = j;
            p->it_prof_incr = i;
#endif
            warn("ITIMER_PROF not implment");
            break;
        default:
            return -EINVAL;
    }
    return 0;
}
