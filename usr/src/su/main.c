/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

/*
 * enter a password in the password file
 * this program should be suid with owner
 * with an owner with write permission on /etc/passwd
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "login.h"

struct passwd *pwd;

// muslの関数を使うが<unistd.h>,<stdlib.h>をインクルードすると
// 対応できない関数が数多くインクルードされるので必要なファイルのみ
// 個別に定義する
uid_t   getuid(void);
int     setuid(uid_t);
gid_t   getgid(void);
int     setgid(gid_t);
void    exit(int);
int     execl(const char *, const char *, ... /* (char  *) NULL */);
char   *getpass(const char *);
char   *crypt(const char *, const char *);

int
main(int argc, char *argv[])
{
    char **p;
    char *login;
    char *password;
    int badsw = 0;
    char *shell = _PATH_DASH;

    if (argc > 1)
        login = argv[1];
    else
        login = "root";
    if ((pwd = getpwnam(login)) == NULL) {
        printf("Unknown id: %s\n",login);
        exit(1);
    }
    // 当面パスワーなしでログイン可能とする
    if (pwd->pw_passwd[0] == '\0' || getuid() == 0)
        goto ok;
    password = getpass("Password:");
    if (badsw || (strcmp(pwd->pw_passwd, crypt(password, pwd->pw_passwd)) != 0)) {
        printf("Sorry\n");
        exit(2);
    }

ok:
    endpwent();
    //printf("su before: uid: %d, gid: %d\n", getuid(), getgid());
    setgid(pwd->pw_gid);
    setuid(pwd->pw_uid);
    //printf("su after : uid: %d, gid: %d\n", getuid(), getgid());
    if (pwd->pw_shell && *pwd->pw_shell)
        shell = pwd->pw_shell;
    for (p = environ; *p; p++) {
        if (strncmp("PS1=", *p, 4) == 0) {
            *p = "PS1=# ";
            break;
        }
    }
    execl(shell, "su", NULL);
    printf("No shell\n");
    exit(3);
}
