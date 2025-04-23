// init: The initial user-level program

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/sysmacros.h>

#define CONSOLE     1

char *sh_argv[] = { "sh", "/etc/inittab", NULL };
char *sh_envp[] = { "PATH=/usr/local/bin:/usr/bin:/bin", "TZ=JST-9", NULL };

int main(int argc, char **argv)
{
    int pid, wpid, status;
    int fd0, fd1, fd2;
    static int init = 1;

    if (open("/dev/tty", O_RDWR) < 0) {
        if (mknod("/dev/tty", (S_IFCHR | 0777), makedev(CONSOLE, 0)) < 0) {
            exit(1);
        }
        fd0 = open("/dev/tty", O_RDWR);
    }
    fd1 = dup(0);  // stdout
    fd2 = dup(0);  // stderr

    //printf("sysin: %d, sysout: %d, syserr: %d\n", fd0, fd1, fd2);

    for (;;) {
        //printf("init: starting sh\n");
        pid = fork();
        if (pid < 0) {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0) {
            //execv("/bin/dash", argv);
            if (init)
                execve("/bin/sh", sh_argv, sh_envp);
                //execve("/bin/sh", 0, sh_envp);
            else
                execve("/usr/bin/dash", 0, 0);
            printf("init: exec sh failed\n");
            exit(1);
        }

        for (;;) {
        // this call to wait() returns if the shell exits,
        // or if a parentless process exits.
            wpid = wait(&status);
            //printf("status: %d\n", status);
            if (wpid == pid) {
                // the shell exited; restart it.
                break;
            } else if (wpid < 0) {
                printf("init: wait returned an error\n");
                exit(1);
            } else {
                // it was a parentless process; do nothing.
            }
        }
        init = 0;
    }

    return 0;
}
