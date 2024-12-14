#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int flag = 0;

void A(int a) {
    printf("PID %d function A got %d\n", getpid(), a);
}

void B(int b) {
    printf("PID %d function B got %d\n", getpid(), b);
}

void C(int c) {
    flag = 1;
    printf("PID %d function C got %d\n", getpid(), c);
}

void D(int d) {
    printf("PID %d function D got %d\n", getpid(), d);
}

void E(int e) {
    printf("PID %d function E got %d\n", getpid(), e);
    printf("bye bye\n");
    exit(0);
}

struct sigaction act_a = { A, 0, 0, 0 };
struct sigaction act_b = { B, 0, 0, 0 };
struct sigaction act_c = { C, 0, 0, 0 };
struct sigaction act_d = { D, 0, 0, 0 };
struct sigaction act_e = { E, 0, 0, 0 };

int main (int argc, char *argv[]) {
    int i, status;
    int cpid, cpid2;
    struct sigaction oldact;

    printf("part1 start\n");
    int ppid = getpid();
    // should print default handler.
    //kill(ppid, SIGUSR1);

    sigaction(SIGUSR1, &act_a, 0);
    // now should print the A message.
    kill(ppid, SIGUSR1);

    sigaction(SIGUSR1, &act_b, &oldact);
    // should print the A message.
    oldact.sa_handler(999);
    // should print the B message.
    kill(ppid, SIGUSR1);

    printf("\npart2 start\n");
    // testing signal sending between different process:
    sigaction(SIGUSR1, &act_c, 0);

    if ((cpid = fork()) < 0) {
        perror("fork");
        exit(1);
    } else if (cpid == 0) { // this is the child
        sigaction(SIGUSR1, &act_c, 0);  // forkするとSIG_IGN以外はSIG_DFLにクリアされるので再設定が必要
        while(!flag); // waiting for signal from parent_pid
        printf("PID %d got signal and sends signal to PID %d\n", getpid(), ppid);
        kill(ppid, SIGUSR1);
        exit(0);
    } else  { // this is the parent
        printf("PID %d sends signal to PID %d\n", ppid, cpid);
        // should only accour once
        kill(cpid, SIGUSR1);
        kill(cpid, SIGUSR1);
        while(!flag);
        printf("PID %d got signal from PID %d\n", ppid, cpid);
        wait(0);
    }

    printf("\npart3 start\n");
    // now test all signals
    sigaction(SIGUSR1, &act_d, 0);
    sigaction(SIGUSR2, &act_e, 0); // this should make the child exit, else go to infinite loop

    if ((cpid2 = fork()) < 0) {
        perror("fork 2");
        exit(1);
    } else if (cpid2) {
        sleep(1);
        kill(cpid2, SIGUSR1);
        sleep(1);
        kill(cpid2, SIGUSR2);
        wait(&status);
    } else {
        sigaction(SIGUSR1, &act_d, 0);
        sigaction(SIGUSR2, &act_e, 0);
        while(1);
    }

    printf("\nall ok\n");
    fflush(stdout);
    _exit(0);
}
