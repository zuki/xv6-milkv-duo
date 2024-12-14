#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

volatile int got_signal = 0;

static void hdl(int sig) {
    got_signal = 1;
}

int main(void) {
    sigset_t mask;
    sigset_t org_mask;
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = hdl;
    if (sigaction(SIGTERM, &act, 0)) {
        perror("sigaction");
        exit(1);
    }

    if (sigemptyset(&mask) < 0) {
        perror("sigemptyset");
        exit(1);
    }
    //printf("mask1: 0x%x\n", mask.__bits[0]);
    if (sigaddset(&mask, SIGTERM) < 0) {
        perror("sigaddset");
        exit(1);
    }
    //printf("mask2: 0x%x\n", mask.__bits[0]);
    if (sigprocmask(SIG_BLOCK, &mask, &org_mask) < 0) {
        perror("sigprocmask");
        exit(1);
    }
    kill(getpid(), SIGTERM);
    //printf("org_mask: 0x%x\n", org_mask.__bits[0]);
    sleep(3);
    if (sigprocmask(SIG_SETMASK, &org_mask, NULL) < 0) {
        perror("sigprocmask 2");
        exit(1);
    }
    sleep(1);
    if (got_signal)
        puts("Got signal!");
    else
        puts("Wrong done");

    fflush(stdout);
    _exit(0);
}
