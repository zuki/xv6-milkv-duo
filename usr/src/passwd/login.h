#ifndef INC_USER_LOGIN_H
#define INC_USER_LOGIN_H

#define    _PATH_PASSWD     "/etc/passwd"
#define    _PATH_GROUP      "/etc/group"
#define    _PATH_TTY        "/dev/tty"
#define    _PATH_UTMP       "/etc/utmp"
#define    _PATH_WTMP       "/usr/adm/wtmp"
#define    _PATH_DASH       "/usr/bin/dash"
#define    TTYGRPNAME       "tty"        /* name of group to own ttys */

extern char **environ;

struct passwd {
    char    *pw_name;       /* user name */
    char    *pw_passwd;     /* encrypted password */
    int      pw_uid;        /* user uid */
    int      pw_gid;        /* user gid */
    char    *pw_gecos;      /* Honeywell login info */
    char    *pw_dir;        /* home directory */
    char    *pw_shell;      /* default shell */
};

struct group {
    char    *gr_name;       /* group name */
    char    *gr_passwd;     /* group password */
    int      gr_gid;        /* group id */
    char    **gr_mem;       /* group members */
};

struct utmp {
    char    ut_line[8];     /* tty name */
    char    ut_name[8];     /* user id */
    long    ut_time;        /* time on */
};

// login/getgrent.c
void setgrent(void);
void endgrent(void);
struct group *getgrent(void);
struct group *getgrnam(char *);

// login/getpwent.c
void endpwent(void);
void setpwent(void);
struct passwd *getpwent(void);
struct passwd *getpwnam(char *);

#endif
