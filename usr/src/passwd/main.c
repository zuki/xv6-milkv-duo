/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

/*
 * enter a password in the password file
 * this program should be suid with owner
 * with an owner with write permission on /etc/passwd
 */
#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "login.h"

char    temp[]     = "/etc/ptmp";
struct passwd *pwd;
char    *pw;
char    pwbuf[10];
char    buf[512];

// <unistd.h>
char    *getpass(const char *);
char    *getlogin(void);
pid_t    getpid(void);
uid_t    getuid(void);
gid_t    getgid(void);
int      access(const char *, int);
size_t   read(int, void *, size_t);
ssize_t  write(int, const void *, size_t);
int      unlink(const char *);
int      close(int);
char *crypt(const char *, const char *);

// <time.h>
time_t   time(time_t *);

// <fcntl.h>
int      open(const char *, int);
int      creat(const char *, mode_t);

// <stdlib.h>
void     exit(int);

int
main(int argc, char *argv[])
{
    char *p;
    int i, c, pwlen;
    char saltc[2];
    time_t salt;
    int uid, fi, fo;
    int insist = 0;     // 試行回数（2回まで再試行）
    int ok, flags;      // ok: 有効なパスワードか, flags: パスワード使用文字種
    FILE *tf;
    char *uname;

    if (argc < 2) {
        if ((uname = getlogin()) == NULL) {
            printf ("Usage: passwd user\n");
            goto bex;
        } else {
            printf("Changing password for %s\n", uname);
        }
    } else {
        uname = argv[1];
    }
    while(((pwd = getpwent()) != NULL)
        && (strcmp(pwd->pw_name, uname) != 0));
    uid = getuid();
    if ((pwd == NULL) || (uid != 0 && uid != pwd->pw_uid)) {
        printf("Permission denied.\n");
        goto bex;
    }
    endpwent();
    if (pwd->pw_passwd[0] && uid != 0) {
        strcpy(pwbuf, getpass("Old password:"));
        pw = crypt(pwbuf, pwd->pw_passwd);
        if (strcmp(pw, pwd->pw_passwd) != 0) {
            printf("Sorry.\n");
            goto bex;
        }
    }

tryagn:
    strcpy(pwbuf, getpass("New password:"));
    pwlen = strlen(pwbuf);
    if (pwlen == 0) {
        printf("Password unchanged.\n");
        goto bex;
    }
    ok = 0;
    flags = 0;
    p = pwbuf;
    while (c = *p++){
        if (c >= 'a' && c<='z') flags |= 2;
        else if(c>='A' && c<='Z') flags |= 4;
        else if(c>='0' && c<='9') flags |= 1;
        else flags |= 8;
    }
    if (flags >=7 && pwlen >= 4)
        ok = 1;     // 英大小数字（特殊文字） + 4文字
    if (((flags == 2) || (flags == 4)) && pwlen >= 6)
        ok = 1;     // 英大/小のみ + 6文字以上
    if (((flags == 3) || (flags == 5) || (flags == 6)) && pwlen >=5 )
        ok = 1;     // 英小数字/英大数/英大小 + 5文字以上

    if ((ok == 0) && (insist < 2)) {
        if (flags == 1)
            printf("Please use at least one non-numeric character.\n");
        else
            printf("Please use a longer password.\n");
        insist++;
        goto tryagn;
    }

    if (strcmp(pwbuf, getpass("Retype new password:")) != 0) {
        printf ("Mismatch - password unchanged.\n");
        goto bex;
    }

    time(&salt);
    salt += getpid();

    saltc[0] = salt & 077;
    saltc[1] = (salt>>6) & 077;
    for (i = 0; i < 2; i++){
        c = saltc[i] + '.';
        if (c > '9') c += 7;
        if (c > 'Z') c += 6;
        saltc[i] = c;
    }
    pw = crypt(pwbuf, saltc);
    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    if (access(temp, 0) >= 0) {
        printf("Temporary file busy -- try again\n");
        goto bex;
    }
    close(creat(temp, 0600));
    if ((tf = fopen(temp, "w")) == NULL) {
        printf("Cannot create temporary file\n");
        goto bex;
    }

/*
 *    copy passwd to temp, replacing matching lines
 *    with new password.
 */

    while ((pwd = getpwent()) != NULL) {
        if (strcmp(pwd->pw_name, uname) == 0) {
            uid = getuid();
            if (uid != 0 && uid != pwd->pw_uid) {
                printf("Permission denied.\n");
                goto out;
            }
            pwd->pw_passwd = pw;
        }
        fprintf(tf,"%s:%s:%d:%d:%s:%s:%s\n",
            pwd->pw_name,
            pwd->pw_passwd,
            pwd->pw_uid,
            pwd->pw_gid,
            pwd->pw_gecos,
            pwd->pw_dir,
            pwd->pw_shell);
    }
    endpwent();
    fclose(tf);

/*
 *    copy temp back to passwd file
 */

    if ((fi = open(temp, 00)) < 0) {            // O_RDONLY
        printf("Temp file disappeared!\n");
        goto out;
    }
    if((fo = creat(_PATH_PASSWD, 0644)) < 0) {
    //if ((fo = open(_PATH_PASSWD, 02 | 01000, 0666)) < 0) {    // O_WRONLY | O_TRUNC
        printf("Cannot recreat passwd file.\n");
        goto out;
    }
    while ((uid = read(fi, buf, sizeof(buf))) > 0) write(fo, buf, uid);
out:
    unlink(temp);
bex:
    exit(1);
}
