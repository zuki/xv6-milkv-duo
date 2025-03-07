#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

char buf[512];

void wc(int fd, char *name)
{
    int i, n;
    int l, w, c, inword;

    l = w = c = 0;
    inword = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (i = 0; i < n; i++) {
            c++;
            if (buf[i] == '\n')
                l++;
            if (strchr(" \r\t\n\v", buf[i]))
                inword = 0;
            else if (!inword) {
                w++;
                inword = 1;
            }
        }
    }

    if (n < 0){
        fprintf(stderr, "wc: read error\n");
        exit(1);
    }
    fprintf(stdout, "%d %d %d %s\n", l, w, c, name);
}

int main(int argc, char *argv[])
{
    int fd, i;

    if (argc <= 1) {
        wc(0, "");
        return 0;
    }

    for (i = 1; i < argc; i++) {
        if ((fd = open(argv[i], O_RDONLY)) < 0) {
            fprintf(stderr, "wc: cannot open %s\n", argv[i]);
            //fflush(stderr);
            //_exit(1);
            return 1;
        }
        wc(fd, argv[i]);
        close(fd);
    }
    return 0;
}
