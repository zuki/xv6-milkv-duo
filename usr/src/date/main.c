#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

char * const wdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void print_now(void) {
    time_t t = time(NULL) + 9 * 60 * 60;
    struct tm *tm = gmtime(&t);

    //fprintf(stdout, "%04d-%02d-%02d (%s) %02d:%02d:%02d JST\n",
    printf("%04d-%02d-%02d (%s) %02d:%02d:%02d JST\n",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        wdays[tm->tm_wday], tm->tm_hour, tm->tm_min, tm->tm_sec);
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        print_now();
    } else if (argc == 2) {
        struct timespec tp;
        int t = atoi(argv[1]);
        tp.tv_sec = (time_t)t;
        tp.tv_nsec = 0;
        if (clock_settime(CLOCK_REALTIME, &tp) < 0) {
            fprintf(stderr, "failed set date\n");
        } else {
            print_now();
        }
    }

    return 0;
}
