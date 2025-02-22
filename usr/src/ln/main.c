#include <stdio.h>
#include <unistd.h>
#include <termios.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: ln old new\n");
        tcdrain(2);
        _exit(1);
    }

    if (link(argv[1], argv[2]) < 0) {
        fprintf(stderr, "link %s %s: failed\n", argv[1], argv[2]);
        tcdrain(2);
        _exit(1);
    }

    //return 0;
    tcdrain(1);
    _exit(0);
}
