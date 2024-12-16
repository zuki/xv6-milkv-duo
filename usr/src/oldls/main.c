#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>

#define DIRSIZE  32
#define MIN(a, b)   ((a) <= (b) ? (a) : (b))

char *fmtname(char *path)
{
    static char buf[DIRSIZE+1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    //fprintf(stderr, "path: %s, len: %ld : p: %s, len: %ld\n", path, strlen(path), p, strlen(p));
    //size_t len = MIN(strlen(p), DIRSIZE);
    //fprintf(stderr, "path: %s, len: %ld, buf: %p\n", path, len, buf);
    //memset(buf, 0, DIRSIZE+1);
    //memcpy(buf, p, len);
    size_t len = MIN(strlen(p), DIRSIZE);
    memmove(buf, p, len);
    buf[len] = 0;
    return buf;
}

void ls(char *path, int indir)
{
    int fd;
    DIR *dir;
    struct dirent *de;
    struct stat st;
    //fprintf(stderr, "path: %s, indir: %d\n", path, indir);
    if (path == "") return;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    //fprintf(stderr, "indir: %d, path: '%s', dev: %ld, ino: %ld, mode: 0x%x, dir?: %d\n", indir, path, st.st_dev, st.st_ino, st.st_mode, S_ISDIR(st.st_mode));

    close(fd);

    if (S_ISDIR(st.st_mode) && indir) {
        dir = opendir(path);
        if (!dir) {
            fprintf(stderr, "ls: cannot open directory %s\n", path);
            return;
        }
        while ((de = readdir(dir))) {
            ls(de->d_name, 1);
        }
        closedir(dir);
    } else {
        printf("%03o %3ld %10ld %s \n", (st.st_mode & 0777), st.st_ino, st.st_size, fmtname(path));
    }
}

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2) {
        ls(".", 0);
        return 0;
    }

    for (i=1; i<argc; i++)
        ls(argv[i], 0);

    //return 0;
    _exit(0);
}
