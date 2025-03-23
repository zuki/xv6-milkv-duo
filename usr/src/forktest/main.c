#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

int ok = 0, ng = 0;

static void fork_test0(void) {
    printf("[00] no fork\n");

    printf("this is child\n");
    printf("23=%d\n", 23);

    int *p1 = mmap((void *)0, 16, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("mmap failed\n");
        ng++;
        return;
    }
    p1[0] = 23;
    printf("p1[0]=%d\n", p1[0]);
    munmap(p1, 16);
    printf("00: ok\n");
    ok++;
}

static void fork_test1(void) {
    printf("[01] puts\n");

    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        ng++;
        return;
    }
    if (pid == 0) {
        printf("this is child\n");
        exit(0);
    }
    int status;
    wait(&status);
    printf("01: ok, status=%d\n", status);
    ok++;
}

static void fork_test2(void) {
    printf("[01] printf\n");

    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        ng++;
        return;
    }
    if (pid == 0) {
        printf("Hello %s\n", "work");
        exit(0);
    }
    int status;
    wait(&status);
    printf("02: ok, status=%d\n", status);
    ok++;
}

static void fork_test3(void) {
    printf("[01] mmap\n");

    int *p1 = mmap((void *)0, 16, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("mmap failed\n");
        ng++;
        return;
    }

    p1[0] = 23;

    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        ng++;
        return;
    }
    if (pid == 0) {
        printf("child: p1[0]=%d\n", p1[0]);
        exit(0);
    }
    int status;
    wait(&status);
    msync(p1, 16, MS_ASYNC);
    printf("parent: p1[0]=%d\n", p1[0]);
    munmap(p1, 16);
    printf("03: ok, status=%d\n", status);
    ok++;
}

int main(void) {
    fork_test0();
    fork_test1();
    fork_test2();
    fork_test3();
    printf("fork test:  ok: %d, ng: %d\n", ok, ng);
    return 0;
}
