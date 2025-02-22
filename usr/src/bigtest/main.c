#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>

int main(void)
{
    char buf[512];
    int fd, i, sectors;

    fd = open("bigfile.txt", O_CREAT | O_WRONLY, 0644);
    if (fd < 0){
        printf("big: cannot open bigfile.txt for writing\n");
        exit(1);
    }

    sectors = 0;
    while (1) {
        *(int*)buf = sectors;
        int cc = write(fd, buf, 512);
        if (cc <= 0)
            break;
        sectors++;
        if (sectors % 100 == 0) {
            if (sectors % 8000 == 0)
                printf("\n");
            printf(".");
            fflush(stdout);
        }
        if (sectors > 10240)     // 5MB
            break;
    }

    printf("\nwrote %d sectors\n", sectors);
    fflush(stdout);

    close(fd);
    fd = open("bigfile.txt", O_RDONLY);
    if (fd < 0) {
        printf("big: cannot re-open bigfile.txt for reading\n");
        exit(1);
    }
    for (i = 0; i < sectors; i++) {
        int cc = read(fd, buf, sizeof(buf));
        if (cc <= 0) {
            printf("big: read error at sector %d\n", i);
            exit(1);
        }
        if (*(int*)buf != i) {
            printf("big: read the wrong data (%d) for sector %d\n",
                *(int*)buf, i);
            exit(1);
        }
    }
    close(fd);

    printf("read; ok\n");

    if (unlink("bigfile.txt")) {
        perror("bigfile unlink");
        exit(1);
    }

    printf("done; ok\n");

    tcdrain(1);
    _exit(0);
    //return 0;
}
