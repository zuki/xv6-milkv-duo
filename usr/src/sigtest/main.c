#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int cpid[5];
int j;

void sig_catcher(int sig) {
    printf("PID %d caught sig %d, j %d\n", getpid(), sig, j);
    if (j > -1) {
        kill(cpid[j], SIGINT);
    }

}

int main(void) {
    int i, zombie, status, pid;
    //sigset_t mask;

    struct sigaction action = { sig_catcher, 0, 0, 0 };
    sigaction(SIGINT, &action, 0);

    for (i = 0; i < 5; i++) {
        if ((pid = fork()) == 0) {
            printf("PID %d ready\n", getpid());
            j = i - 1;
            pause(); // wait for signal, SYS_ppoll(0, 0, 0, 0)
        /*
            sigprocmask(0, 0, &mask);
            sigdelset(&mask, SIGINT);
            int ret = sigsuspend(&mask);
            printf("ret=%d\n", ret);
        */
            exit(0); // end process (become a zombie)
        } else {
            cpid[i] = pid;
        }
    }
    sleep(2);
    kill(cpid[4], SIGINT);
    for (i = 0; i < 5; i++) {
        zombie = wait(&status);
        printf("pid %d is dead\n", zombie);
    }

    return 0;
}
