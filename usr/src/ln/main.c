#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: ln old new\n");
        return 0;
    }

    if (link(argv[1], argv[2]) < 0) {
        fprintf(stderr, "link %s %s: failed\n", argv[1], argv[2]);
        return -1;
    }

    return 0;
}
