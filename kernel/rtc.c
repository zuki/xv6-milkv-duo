#include <common/types.h>
#include <defs.h>
#include <cv180x.h>
#include <io.h>
#include <printf.h>
#include <linux/time.h>
#include <errno.h>

extern struct timespec xtime;

static void rtc_calibration(void)
{
    int result = -1;
    uint32_t offset = 0x80;
    uint32_t ctime, cvalue, ftime, fvalue, ftune = 0x100;
    uint32_t freq_int, freq_frac;
    double freq;
    int try = 8;

    /* 1. 粗調整較正 */
    /* 1.1 RTC_ANA_SEL_FTUNE[31] = 0, RTC_ANA_CALIB[15:0] = 0x100 */
    write32(RTC_ANA_CALIB, (read32(RTC_ANA_CALIB) & ~0x8000ffff) | ftune);
    /* 1.4 RTC_FC_COARSE_VALUEを調整 */
    while (try-- > 0)
    {
        /* 1.3 粗調整を開始 */
        write32(RTC_FC_COARSE_EN, 1);
        ctime = read32(RTC_FC_COARSE_CAL) >> 16;
        //debug("ctime: %d", ctime);
        while ((read32(RTC_FC_COARSE_CAL) >> 16) <= ctime) {}
        write32(RTC_FC_COARSE_EN, 0);
        cvalue = read32(RTC_FC_COARSE_CAL) & 0xffff;
        //debug("[%d] cvalue: %d", try, cvalue);
        if (cvalue  > 770) {
            ftune += offset;
        } else if (cvalue < 755) {
            ftune -= offset;
        } else {
            //debug("coarse calib ok! cvalue=%d", cvalue);
            result = 0;
            break;
        }
        write32(RTC_ANA_CALIB, (read32(RTC_ANA_CALIB) & ~0xffff) | ftune);
        offset >>= 1;
        //debug("ftune: 0x%08x, offset: %d, ana_calib: 0x%08x", ftune, offset, read32(RTC_ANA_CALIB));
        delayus(2000);
    }

    if (result) {
        warn("coarse calib failed");
    }

    /* 2. 微調整較正 */
    /* 2.1 RTC_SEC_PULSE_GEN[31]=0, fc_fine_en=1*/
    write32(RTC_SEC_PULST_GEN, read32(RTC_SEC_PULST_GEN) & ~(1 << 31));
    write32(RTC_FC_FINE_EN, 1);
    /* 2.2 RTC_FC_FINE_TIMEをポーリング */
    ftime = read32(RTC_FC_FINE_CAL) >> 24;
    while((read32(RTC_FC_FINE_CAL) >> 24) <= ftime) {}
    /* 2.3 サンプリング値を取得 */
    fvalue = read32(RTC_FC_FINE_CAL) & 0xffffff;
    /* 2.4 32KHz周波数の決定 */
    freq = (256.0 / ((double)fvalue * 40.0)) * 1000000000.0;
    freq_int = (uint32_t)freq;
    freq_frac = (uint32_t)((freq - (double)freq_int) * 256.0);
    /* 2.5 RTC_SEC_PULSE_GENに書き込み */
    write32(RTC_SEC_PULST_GEN, (freq_int << 8) | (freq_frac & 0xff));
    write32(RTC_FC_FINE_EN, 0);
    //debug("fine calib: freq int=%d, frac=%d", freq_int, freq_frac);
}

void set_second_clock(uint32_t t)
{
    write32(RTC_SET_SEC_CNTR_VALUE, t);
    write32(RTC_SET_SEC_CNTR_TRIG, 1);
    while(read32(RTC_SEC_CNTR_VALUE) < t) {};
    xtime.tv_sec = (time_t)t;
    xtime.tv_nsec = 0;
}

time_t get_second_clock(void)
{
    return (time_t)read32(RTC_SEC_CNTR_VALUE);
}

void rtc_init(void)
{
    rtc_calibration();

    write32(RTC_POR_DB_MAGIC_KEY, 0x5af0);
    set_second_clock(1734229806);
    //write32(RTC_SET_SEC_CNTR_VALUE, 1734229806);   // 2024-12-15 11:30:06 JST のunix時間
    //write32(RTC_SET_SEC_CNTR_TRIG, 1);
    //delayms(500);
    //debug("RTC_SEC_CNTR_VALUE: %d", read32(RTC_SEC_CNTR_VALUE));
    //while(read32(RTC_SEC_CNTR_VALUE) < 1734229806) {};
    write32(RTC_PWR_DET_COMP, read32(RTC_PWR_DET_COMP) | 1);
    write32(RTC_PWR_DET_SEL, read32(RTC_PWR_DET_SEL) | 1);
    write32(RTC_POR_RST_CTRL, read32(RTC_POR_RST_CTRL) | 0x2);
    write32(RTC_EN_PWR_VBAT_DET, read32(RTC_EN_PWR_VBAT_DET) & ~0x4);
    //delayms(2000);
    //debug("current sec: %d", read32(RTC_SEC_CNTR_VALUE));
    //set_second_clock(1734229806);
    //debug("current sec: %ld", get_second_clock());
    rtc_now();
    //info("rtc_init ok!");
}

static const unsigned char days_in_month[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

static inline int is_leap_year(int year)
{
    return (!(year % 4) && (year % 100)) || !(year % 400);
}

static int month_days(int month, int year)
{
    return days_in_month[month] + (is_leap_year(year) && month == 1);
}

static int rtc_valid_tm(struct tm *tm)
{
    if (tm->tm_year > 199
        || ((unsigned)tm->tm_mon) >= 12
        || tm->tm_mday < 1
        || tm->tm_mday > month_days(tm->tm_mon, tm->tm_year + 1900)
        || ((unsigned)tm->tm_hour) >= 24
        || ((unsigned)tm->tm_min) >= 60
        || ((unsigned)tm->tm_sec) >= 60) {
            error("rtc_valid_tm: invalid\n");
            return -EINVAL;
        }
    return 0;
}

static time_t tm_to_time(const unsigned int year0, const unsigned int mon0,
        const unsigned int day, const unsigned int hour,
        const unsigned int min, const unsigned int sec)
{
    unsigned int mon = mon0 + 1, year = year0 + 1900;

    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int) (mon -= 2)) {
        mon += 12;  /* Puts Feb last since it has leap day */
        year -= 1;
    }

    return ((((time_t)
                (year/4 - year/100 + year/400 + 367*mon/12 + day) + year*365 - 719499
            ) * 24 + hour /* now have hours - midnight tomorrow handled here */
        ) * 60 + min /* now have minutes */
    ) * 60 + sec; /* finally seconds */
}

int rtc_time_to_tm(time_t time, struct tm *tm)
{
    unsigned int month, year;
    unsigned long secs;
    int days;

    /* time must be positive */
    days = time / 86400LL;
    secs = time - (unsigned int) days * 86400;

    /* day of the week, 1970-01-01 was a Thursday */
    tm->tm_wday = (days + 4) % 7;

    year = 1970 + days / 365;
    days -= (year - 1970) * 365
        + LEAPS_THRU_END_OF(year - 1)
        - LEAPS_THRU_END_OF(1970 - 1);
    if (days < 0) {
        year -= 1;
        days += 365 + is_leap_year(year);
    }
    tm->tm_year = year - 1900;
    tm->tm_yday = days + 1;

    for (month = 0; month < 11; month++) {
        int newdays;

        newdays = days - month_days(month, year);
        if (newdays < 0)
            break;
        days = newdays;
    }
    tm->tm_mon = month;
    tm->tm_mday = days + 1;

    tm->tm_hour = secs / 3600;
    secs -= tm->tm_hour * 3600;
    tm->tm_min = secs / 60;
    tm->tm_sec = secs - tm->tm_min * 60;

    tm->tm_isdst = 0;

    return 0;
}

// 現在時刻のepoc秒を返す
time_t rtc_time(time_t *t)
{
    time_t now = get_second_clock();
    if (t)
        *t = now;
    return now;
}

int rtc_gettime(struct timespec *tp) {
    tp->tv_sec = get_second_clock();
    tp->tv_nsec = 0L;

    trace("tp_sec: %lld", tp->tv_sec);

    return 0;

}

int rtc_settime(const struct timespec *tp)
{
    set_second_clock(tp->tv_sec);
    return 0;
}

void rtc_strftime(struct tm *tm)
{
   printf("%04d-%02d-%02d %02d:%02d:%02d JST\n",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void rtc_now(void)
{
    struct tm tm;
    time_t jst = rtc_time(NULL) + 9 * 60 * 60;
    rtc_time_to_tm(jst, &tm);
    rtc_strftime(&tm);
}
