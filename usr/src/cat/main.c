#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

char buf[512];

void cat(int fd)
{
    int n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(1, buf, n) != n) {
            perror("write stdout error");
            return;
        }
    }
    if (n < 0) {
        perror("read stdin error");
    }
}

int main(int argc, char *argv[])
{
    int fd, i;

    if (argc <= 1) {
        cat(0);
        //return 0;
        tcdrain(1);
        _exit(0);
    }

    for (i = 1; i < argc; i++) {
        if ((fd = open(argv[i], O_RDONLY)) < 0) {
            perror("open error");
            continue;
        }
        cat(fd);
        close(fd);
    }
    //return 0;
    tcdrain(1);
    _exit(0);
}
