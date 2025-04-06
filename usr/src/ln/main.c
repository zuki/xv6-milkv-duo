#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: ln [-s] old new\n");
        return 0;
    }

    if (argc == 4) {
        if (argv[1][0] == '-' && argv[1][1] == 's') {
            if (symlink(argv[2], argv[3]) < 0) {
                fprintf(stderr, "symlink %s %s: failed\n", argv[2], argv[3]);
                return -1;
            }
        } else {
            fprintf(stderr, "bad option: %x\n", argv[1]);
            return -1;
        }
    } else {
        if (link(argv[1], argv[2]) < 0) {
            fprintf(stderr, "link %s %s: failed\n", argv[1], argv[2]);
            return -1;
        }
    }

    return 0;
}
