/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

#include <stdio.h>
#include "login.h"

#define    CL       ':'
#define    CM       ','
#define    NL       '\n'
#define    MAXGRP   100

int strcmp(const char *, const char *);     // <string.h>

int atoi(const char *);        // <stdlib.h>

static FILE *grf = NULL;
static char line[BUFSIZ+1];
static struct group group;
static char *gr_mem[MAXGRP];

void setgrent(void)
{
    if (!grf)
        grf = fopen(_PATH_GROUP, "r");
    else
        rewind(grf);
}

void endgrent(void)
{
    if (grf){
        fclose(grf);
        grf = NULL;
    }
}

// 文字cをnullに変換して、cの次のポインタを返す: 文字cで行を分割
static char *
grskip(char *p, int c)
{
    while (*p && *p != c) ++p;
    if (*p) *p++ = 0;
    return p;
}

struct group *
getgrent(void)
{
    char *p, **q;

    if (!grf && !(grf = fopen(_PATH_GROUP, "r")))
        return NULL;
    if (!(p = fgets(line, BUFSIZ, grf)))
        return NULL;
    // name:password:gid:mem1,mem2[,...]
    group.gr_name = p;                          // name
    group.gr_passwd = p = grskip(p, CL);        // password
    group.gr_gid = atoi(p = grskip(p, CL));     // gid
    group.gr_mem = gr_mem;
    p = grskip(p, CL);
    grskip(p, NL);
    q = gr_mem;
    while (*p){
        *q++ = p;
        p = grskip(p, CM);                      // mem1\0nem1
    }
    *q = NULL;                                  // mem1\0mem2\0
    return &group;
}

struct group *
getgrnam(char *name)
{
    struct group *group;

    setgrent();
    while (((group = getgrent()) != NULL) && strcmp(group->gr_name, name) != 0);
    endgrent();
    return group;
}
