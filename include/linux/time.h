#ifndef INC_LINUX_TIME_H
#define INC_LINUX_TIME_H

#include <common/types.h>

//#define HZ 100
//#define INITIAL_JIFFIES ((uint64_t)-300 * HZ)                // itimerが動かず
//#define INITIAL_JIFFIES ((uint64_t)(uint32_t)(-300 * HZ))    // clock割り込みしない
//#define INITIAL_JIFFIES 0UL

struct timeval {
    time_t      tv_sec;     /* 秒 */
    suseconds_t tv_usec;    /* マイクロ秒 */
};

struct timespec {
    time_t  tv_sec;         /* 秒 */
    long    tv_nsec;        /* ナノ秒 */
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};


struct itimerval {
    struct timeval it_interval; /* 周期的タイマーのInterval */
    struct timeval it_value;    /* 次の時間切れまでの時間 */
};

#if 0
struct timer_list {
    struct list_head list;
    uint64_t expires;
    uint64_t data;
    void (*function)(uint64_t);
};
#endif

extern uint64_t jiffies;
extern struct timespec xtime;

#define CURRENT_TIME (xtime.tv_sec)

#define CLOCK_REALTIME           0  /* 実時間を計測する設定可能なシステム全体で一意な時間 */
#define CLOCK_MONOTONIC          1  /* ある開始時点から単調増加するクロック */
#define CLOCK_PROCESS_CPUTIME_ID 2  /* プロセス単位の CPU タイムクロック */
#define CLOCK_THREAD_CPUTIME_ID  3  /* スレッド固有の CPU タイムクロック */
#define CLOCK_MONOTONIC_RAW      4  /* NTPの調整やadjtime(3)の段階的な調整の影響を受けないCLOCK_MONOTONIC */
#define CLOCK_REALTIME_COARSE    5  /* 高速だが精度が低い CLOCK_REALTIME */
#define CLOCK_MONOTONIC_COARSE   6  /* 高速だが精度が低い CLOCK_MONOTONIC */
#define CLOCK_BOOTTIME           7  /* システムがサスペンドされている時間も含まれるCLOCK_MONOTONIC */
#define CLOCK_REALTIME_ALARM     8  /* 実時間を計測する設定不能なシステム全体で一意な時間 */
#define CLOCK_BOOTTIME_ALARM     9
#define CLOCK_SGI_CYCLE         10
#define CLOCK_TAI               11

// タイマー種別
#define ITIMER_REAL             0   /* 実時間タイマー: SIGALRM */
#define ITIMER_VIRTUAL          1   /* プロセスタイマー: SIGVTARM */
#define ITIMER_PROF             2   /* プロファイラ用タイマー: SIGPROF */

#define TIME_UTC                1

#if 0
// FIXEME: externは必要？
// タイマーの登録
extern void add_timer(struct timer_list *timer);
// タイマーのキャンセル
extern int del_timer(struct timer_list *timer);
// すでに実行中の場合は終了を待つ
#define del_timer_sync(t)   del_timer(t)
#define sync_timers()       do { } while (0)

// expireの変更
int mod_timer(struct timer_list *timer, uint64_t expires);

static inline void init_timer(struct timer_list * timer)
{
	timer->list.next = timer->list.prev = NULL;
}

static inline int timer_pending(const struct timer_list * timer)
{
	return timer->list.next != NULL;
}

#define time_after(a,b)     ((int64_t)(b) - (int64_t)(a) < 0)
#define time_before(a,b)    time_after(b,a)

#define time_after_eq(a,b)  ((int64_t)(a) - (int64_t)(b) >= 0)
#define time_before_eq(a,b) time_after_eq(b,a)

void init_timervecs(void);
void run_timer_list(void);
long getitimer(int, struct itimerval *);
void it_real_fn(uint64_t);
long setitimer(int, struct itimerval *, struct itimerval *);
#endif

#endif
