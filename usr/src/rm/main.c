#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2) {
        fprintf(stderr, "wrong argc: %d\n", argc);
        fprintf(stderr, "Usage: rm files...\n");
        fflush(stderr);
        //return -1;
        _exit(1);
    }

    for (i = 1; i < argc; i++) {
        if (unlink(argv[i]) < 0) {
            fprintf(stderr, "rm: %s failed to delete\n", argv[i]);
            fflush(stderr);
            break;
        }
    }

    //return 0;
    _exit(0);
}
