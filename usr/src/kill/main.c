#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <termios.h>

int main(int argc, char **argv)
{
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: kill pid...\n");
        tcdrain(2);
        _exit(1);
        //return -1;
    }
    for (i=1; i<argc; i++)
        kill(atoi(argv[i]), SIGKILL);

    //return 0;
    tcdrain(1);
    _exit(0);
}
