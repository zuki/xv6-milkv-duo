#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2) {
        fprintf(stderr, "wrong argc: %d\n", argc);
        fprintf(stderr, "Usage: mkdir files...\n");
        return -1;
    }

    for (i = 1; i < argc; i++) {
        if (mkdir(argv[i], 0755) < 0){
            fprintf(stderr, "mkdir: %s failed to create\n", argv[i]);
            fflush(stderr);
            break;
        }
    }

    return 0;
}
