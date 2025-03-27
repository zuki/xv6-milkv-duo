/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

/*
 * login [ name ]
 */
#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "login.h"

#define SCPYN(a, b)    strncpy(a, b, sizeof(a))

#define TCSETS      0x5402

struct passwd nouser = {"", "nope"};
struct termios termios;
struct  utmp utmp;
char    minusnam[16] = "-";
char    homedir[64] = "HOME=";
char    *envinit[] = {homedir, "PATH=:/bin:/usr/bin", 0};
struct passwd *pwd;

// <unistd.h>
char    *getpass(const char *);
char *ttyname(int);
int close(int);
unsigned int alarm(unsigned int);
int chdir(const char *);
int ttyslot(void);
off_t lseek(int, off_t, int);
ssize_t write(int, const void *, size_t);
int chown(const char *, uid_t, gid_t);
int setuid(uid_t);
int setgid(gid_t);
int execlp(const char *, const char *, ... /* (char  *) NULL */);
char *crypt(const char *, const char *);

// <stdlib.h>
void exit(int);

// <time.h>
time_t time(time_t *);

// <fcntl.h>
int open(const char *, int);

char **environ;

int
main(int argc, char **argv)
{
    char *namep;
    int t, f, c;
    char *ttyn;

    alarm(60);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    if (!tcgetattr(0, &termios)) {
        termios.c_cc[VERASE] = '#';
        termios.c_cc[VKILL] = '@';
        tcsetattr(0, TCSETS, &termios);
    }

    for (t = 3; t < 20; t++)
        close(t);

    ttyn = ttyname(0);
    if (ttyn == 0)
        ttyn = _PATH_TTY;

loop:
    SCPYN(utmp.ut_name, "");
    if (argc > 1) {
        SCPYN(utmp.ut_name, argv[1]);
        argc = 0;
    }
    while (utmp.ut_name[0] == '\0') {
        namep = utmp.ut_name;
        printf("login: ");
        fflush(stdout);
        while ((c = getchar()) != '\n') {
            if(c == ' ')
                c = '_';
            if (c == EOF)
                exit(0);
            if (namep < utmp.ut_name+8)
                *namep++ = c;
        }
        *namep = 0;
    }

    if ((pwd = getpwnam(utmp.ut_name)) == NULL) {
        pwd = &nouser;
    }

    if (*pwd->pw_passwd != '\0') {
        namep = crypt(getpass("Password:"), pwd->pw_passwd);
        if (strcmp(namep, pwd->pw_passwd)) {
            printf("Login incorrect\n");
            goto loop;
        }
    }
    if (chdir(pwd->pw_dir) < 0) {
        printf("No directory\n");
        goto loop;
    }
/*
    time(&utmp.ut_time);
    t = ttyslot();
    if (t > 0 && (f = open(_PATH_UTMP, 1)) >= 0) {
        lseek(f, (long)(t*sizeof(utmp)), 0);
        SCPYN(utmp.ut_line, index(ttyn+1, '/')+1);
        write(f, (char *)&utmp, sizeof(utmp));
        close(f);
    }
    if (t > 0 && (f = open(_PATH_WTMP, 1)) >= 0) {
        lseek(f, 0L, 2);
        write(f, (char *)&utmp, sizeof(utmp));
        close(f);
    }
*/
    chown(ttyn, pwd->pw_uid, pwd->pw_gid);
    setgid(pwd->pw_gid);
    setuid(pwd->pw_uid);
    if (*pwd->pw_shell == '\0')
        pwd->pw_shell = _PATH_DASH;
    environ = envinit;
    strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
    if ((namep = rindex(pwd->pw_shell, '/')) == NULL)
        namep = pwd->pw_shell;
    else
        namep++;
    strcat(minusnam, namep);
    alarm(0);
    umask(02);

    signal(SIGQUIT, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    execlp(pwd->pw_shell, minusnam, NULL);
    printf("No shell\n");
    exit(0);
}
