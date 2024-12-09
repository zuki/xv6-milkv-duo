// Shell.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
    int type;
};

struct execcmd {
    int type;
    char *argv[MAXARGS];
    char *eargv[MAXARGS];
};

struct redircmd {
    int type;
    struct cmd *cmd;
    char *file;
    char *efile;
    int mode;
    int fd;
};

struct pipecmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct listcmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct backcmd {
    int type;
    struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char*);
void runcmd(struct cmd *) __attribute__((noreturn));

void * malloc1(size_t sz)
{
#define MAXN 16384
    static char mem[MAXN];
    static size_t i = 0;

    if ((i += sz) > MAXN) {
        fprintf(stderr, "malloc1: out of memory\n");
        exit(1);
    }

    //fprintf(stderr, "size: %ld, i: %ld, addr: %p\n", sz, i, &mem[i - sz]);
    return &mem[i - sz];
}

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd)
{
    int p[2];
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0)
        exit(1);

    switch(cmd->type){
    default:
        panic("runcmd");

    case EXEC:
        ecmd = (struct execcmd *)cmd;
        //fprintf(stderr, "exec %s\n", ecmd->argv[0]);
        if (ecmd->argv[0] == 0)
            exit(1);
        execv(ecmd->argv[0], ecmd->argv);
        fprintf(stderr, "exec %s failed\n", ecmd->argv[0]);
        break;

    case REDIR:
        rcmd = (struct redircmd *)cmd;
        close(rcmd->fd);
        if (open(rcmd->file, rcmd->mode) < 0){
            fprintf(stderr, "open %s failed\n", rcmd->file);
            exit(1);
        }
        runcmd(rcmd->cmd);
        break;

    case LIST:
        lcmd = (struct listcmd *)cmd;
        if (fork1() == 0)
            runcmd(lcmd->left);
        wait(0);
        runcmd(lcmd->right);
        break;

    case PIPE:
        pcmd = (struct pipecmd *)cmd;
        if (pipe(p) < 0)
            panic("pipe");
        if (fork1() == 0) {
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
            runcmd(pcmd->left);
        }
        if (fork1() == 0) {
            close(0);
            dup(p[0]);
            close(p[0]);
            close(p[1]);
            runcmd(pcmd->right);
        }
        close(p[0]);
        close(p[1]);
        wait(0);
        wait(0);
        break;

    case BACK:
        bcmd = (struct backcmd *)cmd;
        if (fork1() == 0)
            runcmd(bcmd->cmd);
        break;
    }
    exit(0);
}

int getcmd(char *buf, int nbuf)
{
    fprintf(stderr, "$ ");
    memset(buf, 0, nbuf);
    fgets(buf, nbuf, stdin);
    if (buf[0] == 0) // EOF
        return -1;
    return 0;
}

int main(int argc, char *argv[])
{
    static char buf[100];
    int fd;

    // Ensure that three file descriptors are open.
    while ((fd = open("/console", O_RDWR)) >= 0) {
        if (fd >= 3) {
            close(fd);
            break;
        }
    }

    // Read and run input commands.
    while (getcmd(buf, sizeof(buf)) >= 0) {
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            // Chdir must be called by the parent, not the child.
            buf[strlen(buf) - 1] = 0;  // chop \n
            if (chdir(buf+3) < 0)
                fprintf(stderr, "cannot cd %s\n", buf+3);
            continue;
        }
        if (fork1() == 0) {
            //fprintf(stderr, "forked and run command: %s\n", buf);
            runcmd(parsecmd(buf));
        }
        wait(0);
    }

    return 0;
}

void panic(char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(1);
}

int fork1(void)
{
    int pid;

    pid = fork();
    if (pid == -1)
        panic("fork");
    return pid;
}

struct cmd *execcmd(void)
{
    struct execcmd *cmd;
    //fprintf(stderr, "called execcmd ");
    cmd = malloc1(sizeof(*cmd));
    //fprintf(stderr, "malloc ");
    memset(cmd, 0, sizeof(*cmd));
    //fprintf(stderr, "memset ");
    cmd->type = EXEC;
    //fprintf(stderr, "created\n");
    return (struct cmd *)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
    struct redircmd *cmd;

    cmd = malloc1(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd = fd;
    //fprintf(stderr, "made redircmd\n");
    return (struct cmd *)cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right)
{
    struct pipecmd *cmd;

    cmd = malloc1(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    //fprintf(stderr, "made pipecmd\n");
    return (struct cmd *)cmd;
}

struct cmd *listcmd(struct cmd *left, struct cmd *right)
{
    struct listcmd *cmd;

    cmd = malloc1(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    //fprintf(stderr, "made listcmd\n");
    return (struct cmd *)cmd;
}

struct cmd *backcmd(struct cmd *subcmd)
{
    struct backcmd *cmd;

    cmd = malloc1(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    //fprintf(stderr, "made backcmd\n");
    return (struct cmd *)cmd;
}

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
    char *s;
    int ret;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    if (q)
        *q = s;
    ret = *s;
    switch(*s) {
    case 0:
        break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
        s++;
        break;
    case '>':
        s++;
        if (*s == '>') {
            ret = '+';
            s++;
        }
        break;
    default:
        ret = 'a';
        while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
            s++;
        break;
    }

    if (eq)
        *eq = s;

    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    //fprintf(stderr, "token: q=%s, eq=%s\n", *q, *eq);
    return ret;
}

int peek(char **ps, char *es, char *toks)
{
    char *s;
    int ret;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    ret = (*s && strchr(toks, *s));
    //fprintf(stderr, "ret: %d, peek: s=%s, toks=%s\n", ret, s, toks);
    return ret;
    //return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd *parsecmd(char *s)
{
    char *es;
    struct cmd *cmd;

    es = s + strlen(s);
    cmd = parseline(&s, es);
    peek(&s, es, "");
    if (s != es){
        fprintf(stderr, "leftovers: %s\n", s);
        panic("syntax");
    }
    nulterminate(cmd);
    return cmd;
}

struct cmd* parseline(char **ps, char *es)
{
    struct cmd *cmd;

    //fprintf(stderr, "parseline: ps: %s", *ps);
    cmd = parsepipe(ps, es);
    while (peek(ps, es, "&")) {
        gettoken(ps, es, 0, 0);
        cmd = backcmd(cmd);
    }
    if (peek(ps, es, ";")) {
        gettoken(ps, es, 0, 0);
        cmd = listcmd(cmd, parseline(ps, es));
    }
    //fprintf(stderr, " type: %d, ps1: %s\n", cmd->type, *ps);
    return cmd;
}

struct cmd *parsepipe(char **ps, char *es)
{
    struct cmd *cmd;
    //fprintf(stderr, "parsepipe: ");
    cmd = parseexec(ps, es);
    if (peek(ps, es, "|")) {
        //fprintf(stderr, " | ");
        gettoken(ps, es, 0, 0);
        cmd = pipecmd(cmd, parsepipe(ps, es));
    }
    //fprintf(stderr, "\n");
    return cmd;
}

struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es)
{
    int tok;
    char *q, *eq;
    //fprintf(stderr, "parseredirs:");
    while (peek(ps, es, "<>")) {
        tok = gettoken(ps, es, 0, 0);
        if (gettoken(ps, es, &q, &eq) != 'a')
            panic("missing file for redirection");
        switch(tok) {
        case '<':
            //fprintf(stderr, " < ");
            cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
            break;
        case '>':
            //fprintf(stderr, " > ");
            cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREAT|O_TRUNC, 1);
            break;
        case '+':  // >>
            //fprintf(stderr, " + ");
            cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREAT, 1);
            break;
        }
    }
    //fprintf(stderr, "\n");
    return cmd;
}

struct cmd *parseblock(char **ps, char *es)
{
    struct cmd *cmd;
    //fprintf(stderr, "parseblock: ");

    if (!peek(ps, es, "("))
        panic("parseblock");
    //fprintf(stderr, " ( ");
    gettoken(ps, es, 0, 0);
    cmd = parseline(ps, es);
    if (!peek(ps, es, ")"))
        panic("syntax - missing )");
    //fprintf(stderr, " ) ");
    gettoken(ps, es, 0, 0);
    cmd = parseredirs(cmd, ps, es);
    //fprintf(stderr, "\n");
    return cmd;
}

struct cmd *parseexec(char **ps, char *es)
{
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;
    //fprintf(stderr, "parseexec: ");
    if (peek(ps, es, "("))
        return parseblock(ps, es);

    ret = execcmd();
    cmd = (struct execcmd *)ret;

    argc = 0;
    ret = parseredirs(ret, ps, es);
    //fprintf(stderr, "ps=%s, es=%s", *ps, es);
    while (!peek(ps, es, "|)&;")){
        if ((tok=gettoken(ps, es, &q, &eq)) == 0)
            break;
        if (tok != 'a')
            panic("syntax");
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        //fprintf(stderr, " arv[%d]=%s, eargv[%d]=%s", argc, cmd->argv[argc], argc, cmd->eargv[argc]);
        argc++;
        if (argc >= MAXARGS)
            panic("too many args");
        ret = parseredirs(ret, ps, es);
    }
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;
    //fprintf(stderr, "argc=%d\n", argc);
    return ret;
}

// NUL-terminate all the counted strings.
struct cmd *nulterminate(struct cmd *cmd)
{
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;
    //fprintf(stderr, "nulterminate: ");

    if (cmd == 0) {
        //fprintf(stderr, "\n");
        return 0;
    }

    switch(cmd->type) {
    case EXEC:
        //fprintf(stderr, " exec ");
        ecmd = (struct execcmd *)cmd;
        for (i=0; ecmd->argv[i]; i++) {
            //fprintf(stderr, " %d", i);
            *ecmd->eargv[i] = 0;
        }
        //fprintf(stderr, "\n");
        break;

    case REDIR:
        //fprintf(stderr, " redir ");
        rcmd = (struct redircmd *)cmd;
        nulterminate(rcmd->cmd);
        *rcmd->efile = 0;
        break;

    case PIPE:
        //fprintf(stderr, " pipe ");
        pcmd = (struct pipecmd *)cmd;
        nulterminate(pcmd->left);
        nulterminate(pcmd->right);
        break;

    case LIST:
        //fprintf(stderr, " list ");
        lcmd = (struct listcmd *)cmd;
        nulterminate(lcmd->left);
        nulterminate(lcmd->right);
        break;

    case BACK:
        //fprintf(stderr, " back ");
        bcmd = (struct backcmd *)cmd;
        nulterminate(bcmd->cmd);
        break;
    }
    //fprintf(stderr, "\n");
    return cmd;
}
