#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char buf[512];

void cat(int fd)
{
    int n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(1, buf, n) != n) {
            fprintf(stderr, "cat: write error\n");
            return;
        }
    }
    if (n < 0) {
        fprintf(stderr, "cat: read error\n");
        fflush(stderr);
        return;
    }
}

int main(int argc, char *argv[])
{
    int fd, i;

    if (argc <= 1) {
        cat(0);
        //return 0;
        _exit(0);
    }

    for (i = 1; i < argc; i++) {
        if ((fd = open(argv[i], O_RDONLY)) < 0) {
            fprintf(stderr, "cat: cannot open %s\n", argv[i]);
            fflush(stderr);
            //return 1;
            _exit(1);
        }
        cat(fd);
        close(fd);
    }
    //return 0;
    _exit(0);
}
