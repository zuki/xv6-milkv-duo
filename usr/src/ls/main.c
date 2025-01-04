#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#define DIRSIZ 14

struct v6_dirent {
  uint16_t inum;
  char name[DIRSIZ];
};

char *fmtname(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;
    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--) ;
    p++;

    // Return blank-padded name.
    /*
    if (strlen(p) >= DIRSIZ)
        return p;
    */
    memmove(buf, p, DIRSIZ);
    buf[DIRSIZ] = 0;
    //memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    return buf;
}

char * fmttime(time_t time)
{
    static char mtime_s[12];

    struct tm *tm = localtime(&time);
    sprintf(mtime_s, "%2d %2d %02d:%02d",
        tm->tm_mon + 1, tm->tm_mday,  tm->tm_hour, tm->tm_min);
    return mtime_s;
}

char * fmtmode(mode_t mode)
{
    static char fmod[11];

    if (S_ISDIR(mode)) fmod[0] = 'd';
    else if (S_ISCHR(mode)) fmod[0] = 'c';
    else if (S_ISBLK(mode)) fmod[0] = 'b';
    else if (S_ISFIFO(mode)) fmod[0] = 'f';
    else if (S_ISLNK(mode)) fmod[0] = 'l';
    else if (S_ISSOCK(mode)) fmod[0] = 's';
    else fmod[0] = '-';

    fmod[1] = (S_IRUSR & mode) ? 'r' : '-';
    fmod[2] = (S_IWUSR & mode) ? 'w' : '-';
    fmod[3] = (S_IXUSR & mode) ? 'x' : '-';
    fmod[4] = (S_IRGRP & mode) ? 'r' : '-';
    fmod[5] = (S_IWGRP & mode) ? 'w' : '-';
    fmod[6] = (S_IXGRP & mode) ? 'x' : '-';
    fmod[7] = (S_IROTH & mode) ? 'r' : '-';
    fmod[8] = (S_IWOTH & mode) ? 'w' : '-';
    fmod[9] = (S_IXOTH & mode) ? 'x' : '-';
    if (S_ISUID & mode) fmod[3] = 's';
    if (S_ISGID & mode) fmod[6] = 's';
    if (S_ISVTX & mode) fmod[9] = 't';
    fmod[10] = 0;

    return fmod;
}

char *fmtuser(uid_t uid, gid_t gid)
{
    if (uid == 0 && gid == 0)
        return "root wheel";
    else if (uid == 1000 && gid == 1000)
        return "zuki staff";
    else
        return "anon anon ";
}

void ls(char *path)
{
    char buf[128], *p;
    int fd;
    struct v6_dirent de;
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

#if 0
    if (S_ISREG(st.st_mode)) {
        printf("%s %4ld %s %5ld %s\n", fmtmode(st.st_mode), st.st_ino,
               fmtuser(0, 0), st.st_size, fmtname(path));
        //fflush(stdout);
#endif
     if (S_ISDIR(st.st_mode)) {
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            fprintf(stderr, "ls: path too long\n");
        } else {
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0)
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (stat(buf, &st) < 0) {
                    fprintf(stderr, "ls: cannot stat %s\n", buf);
                    continue;
                }
                printf("%s %4ld %s %5ld %s %s\n", fmtmode(st.st_mode),
                       st.st_ino, fmtuser(0, 0), st.st_size, fmttime(st.st_mtime), fmtname(buf));
                //fflush(stdout);
            }
        }
    } else {
        printf("%s %4ld %s %5ld %s %s\n", fmtmode(st.st_mode), st.st_ino,
               fmtuser(0, 0), st.st_size, fmttime(st.st_mtime), fmtname(path));
        //fflush(stdout);
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        ls(".");
    else
        for (int i = 1; i < argc; i++)
            ls(argv[i]);
    //printf("main ended\n");
#if 1
    fflush(stdout);
    fflush(stderr);
    _exit(0);
#endif
    //return 0;
}
