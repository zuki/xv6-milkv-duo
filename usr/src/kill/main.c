#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>

int main(int argc, char **argv)
{
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: kill pid...\n");
        return -1;
    }
    for (i=1; i<argc; i++)
        kill(atoi(argv[i]), SIGKILL);
    return 0;
}
