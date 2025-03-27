/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

#include <stdio.h>
#include "login.h"

int atoi(const char *);        // <stdlib.h>
int strcmp(const char *, const char *);     // <string.h>
long lseek(int, long, int);

static char EMPTY[] = "";
static FILE *pwf = NULL;
static char line[BUFSIZ+1];
static struct passwd passwd;

void
setpwent(void)
{
    if (pwf == NULL)
        pwf = fopen(_PATH_PASSWD, "r");
    else
        rewind (pwf);
}

void
endpwent(void)
{
    if (pwf != NULL) {
        fclose(pwf);
        pwf = NULL;
    }
}

// ':'を\0で置き換え（行を':'で分割）
static char *
pwskip(char *p)
{
    while (*p && *p != ':')
        ++p;
    if (*p) *p++ = 0;
    return (p);
}

/* /etc/passedから一つstruct passwdを返す */

struct passwd *
getpwent(void)
{
    char *p;

    if (pwf == NULL) {
        if ((pwf = fopen(_PATH_PASSWD, "r")) == NULL)
            return (0);
    }

    p = fgets(line, BUFSIZ, pwf);
    if (p == NULL)
        return (0);
    //printf("off=%ld\n", fseek(pwf, 0, 1));
    // name:passwd:uid:gid:gecos:home:shell
    passwd.pw_name = p;
    p = pwskip(p);
    passwd.pw_passwd = p;
    p = pwskip(p);
    passwd.pw_uid = atoi(p);
    p = pwskip(p);
    passwd.pw_gid = atoi(p);
    p = pwskip(p);
    passwd.pw_gecos = p;
    p = pwskip(p);
    passwd.pw_dir = p;
    p = pwskip(p);
    passwd.pw_shell = p;
    while (*p && *p != '\n') p++;
    *p = '\0';
    return &passwd;
}

/* user nameに一致するstruct passwdを取得する */
struct passwd *
getpwnam(char *name)
{
	struct passwd *passwd;

	setpwent();
	while ((passwd = getpwent()) && strcmp(name, passwd->pw_name));
	endpwent();
	return passwd;
}
