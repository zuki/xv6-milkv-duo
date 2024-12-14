#include <linux/signal.h>

int sigemptyset(sigset_t *set) {
    *set = 0UL;
    return 0;
}

int sigfillset(sigset_t *set) {
    *set = 0xffffffffffffffffUL;
    return 0;
}

int sigaddset(sigset_t *set, int signum) {
    if (signum < 1 || signum >= NSIG) {
        return -1;
    }
    *set |= (1 << (signum - 1));
    return 0;
}

int sigdelset(sigset_t *set, int signum) {
    if (signum < 1 || signum >= NSIG) {
        return -1;
    }

    *set &= ~(1 << (signum - 1));
    return 0;
}

int sigismember(const sigset_t *set, int signum) {
    if (signum < 1 || signum >= NSIG) {
        return -1;
    }

    return (*set & (1 << (signum - 1))) ? 1 : 0;
}

int sigisemptyset(const sigset_t *set) {
    return !*set ? 1 : 0;
}

int signotset(sigset_t *dest, const sigset_t *src) {
    *dest = ~*src;
    return 0;
}

int sigorset(sigset_t *dest, const sigset_t *left, const sigset_t *right) {
    *dest = *left | *right;
    return 0;
}

int sigandset(sigset_t *dest, const sigset_t *left, const sigset_t *right) {
    *dest = *left & *right;
    return 0;
}

int siginitset(sigset_t *dst, sigset_t *src) {
    sigemptyset(dst);
    sigorset(dst, dst, src);
    return 0;
}
