//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include <common/types.h>
#include <common/param.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <common/fs.h>
#include <common/file.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include <defs.h>
#include <proc.h>
#include <linux/ioctl.h>
#include <linux/termios.h>
#include <errno.h>

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

static struct termios termios;

// uartに1文字送信する.
// pritf()と入力文字をエコーする際に呼び出されるが、
// write()からは呼び出されない。
//
void
consputc(int c)
{
    if (c == BACKSPACE) {
        // if the user typed backspace, overwrite with a space.
        uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
    } else if (c == '\n') {
        uartputc_sync('\r');
        uartputc_sync(c);
    } else {
        uartputc_sync(c);
    }
}

struct {
    struct spinlock lock;

    // input
#define INPUT_BUF_SIZE 128
    char buf[INPUT_BUF_SIZE];
    uint32_t r;  // Read index
    uint32_t w;  // Write index
    uint32_t e;  // Edit index
} cons;

#define isecho (termios.c_lflag & ECHO)
#define islbuf (termios.c_lflag & ICANON)

static void set_default_termios(struct termios *termios)
{
    memset(termios, 0, sizeof(struct termios));

    termios->c_oflag = (OPOST|ONLCR);
    termios->c_iflag = (ICRNL|IXON|IXANY|IMAXBEL);
    termios->c_cflag = (CREAD|CS8|B115200);
    termios->c_lflag = (ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOCTL|ECHOKE);

    termios->c_cc[VTIME]   = 0;
    termios->c_cc[VMIN]    = 1;
    termios->c_cc[VINTR]   = (0x1f & 'c');
    termios->c_cc[VQUIT]   = 28;
    termios->c_cc[VEOF]    = (0x1f & 'd');
    termios->c_cc[VSUSP]   = (0x1f & 'z');
    termios->c_cc[VKILL]   = (0x1f & 'u');
    termios->c_cc[VERASE]  = 0177;
    termios->c_cc[VWERASE] = (0x1f & 'w');
}

//
// ユーザがコンソールにwrite()した際はこの関数が呼び出される.
//
int
consolewrite(int user_src, uint64_t src, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        char c;
        if(either_copyin(&c, user_src, src+i, 1) == -1)
            break;
        uartputc(c);
    }

    return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64_t dst, int n)
{
    uint32_t target;
    int c = 0;
    char cbuf;

    target = n;
    acquire(&cons.lock);
    while (n > 0) {
        // wait until interrupt handler has put some
        // input into cons.buffer.
        while (cons.r == cons.w) {
            if (killed(myproc())) {
                release(&cons.lock);
                return -1;
            }
            sleep(&cons.r, &cons.lock);
        }

        c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

        if (c == C('D')) {  // end-of-file
            if (n < target) {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                cons.r--;
            }
            break;
        }

        // copy the input byte to the user-space buffer.
        cbuf = c;

        if (either_copyout(user_dst, dst, &cbuf, 1) == -1)
            break;

        dst++;
        --n;

        if(c == '\n') {
            // a whole line has arrived, return to
            // the user-level read().
            break;
        }
    }
    trace("target: %d, n: %d, c: %x, r: %d, w: %d, e: %d", target, n, c, cons.r, cons.w, cons.e);
    release(&cons.lock);

    return target - n;
}

static int consoleioctl(int fd, uint64_t req, void *argp)
{
    struct proc *p = myproc();
    struct winsize wins = { .ws_row = 24, .ws_col = 80 };

    switch (req) {
        case TCGETS:
            if (copyout(p->pagetable, (uint64_t)argp, (char *)&termios, sizeof(struct termios)) < 0)
                return -EINVAL;
            break;
        case TCSETS:
        case TCSETSW:
        case TCSETSF:
            if (copyin(p->pagetable, (char *)&termios, (uint64_t)argp, sizeof(struct termios)) < 0)
                return -EINVAL;
            trace("iflag: 0x%x, oflag: 0x%x, cflag: 0x%x, lflag: 0x%x",
                term->c_iflag, term->c_oflag, term->c_cflag, term->c_lflag);
            break;
        case TCSBRK:            // TODO: cosole.drainを実装すること
            trace("TCSBRK");
            break;
        case TCFLSH:            // TODO: cosole.flushを実装すること
            trace("TCFLSH");
            break;
        case TIOCGWINSZ:
            if (copyout(p->pagetable, (uint64_t)argp, (char *)&wins, sizeof(struct winsize)) < 0)
                return -EINVAL;
            break;
        case TIOCSWINSZ:
            // Windowサイズ設定: 当面何もしない
            break;
        case TIOCSPGRP:  // TODO: 本来、dev(tty)用なのでp->sgid?
            p->pgid = (pid_t)(uint64_t)argp;
            break;
        case TIOCGPGRP:
            if (copyout(p->pagetable, (uint64_t)argp, (char *)&p->pgid, sizeof(pid_t)) < 0) {
                error("TIOCGPGRP argp: 0x%lx, pgid: %d, size: %d", argp, p->pgid, sizeof(pid_t));
                return -EINVAL;
            }

            break;
        default:
            error("unimplemented req: 0x%lx", req);
            return -EINVAL;
    }
    return 0;
}

//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
    acquire(&cons.lock);

    switch(c){
    case C('P'):  // Print process list.
        procdump();
        break;
    case C('U'):  // Kill line.
        while (cons.e != cons.w && cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n') {
            cons.e--;
            consputc(BACKSPACE);
        }
        break;
    case C('H'): // Backspace
    case '\x7f': // Delete key
        if (cons.e != cons.w) {
            cons.e--;
            consputc(BACKSPACE);
        }
        break;
    default:
        if (c != 0 && cons.e-cons.r < INPUT_BUF_SIZE) {
            c = (c == '\r') ? '\n' : c;

            // echo back to the user.
            consputc(c);

            // store for consumption by consoleread().
            cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

            if (c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE) {
                // wake up consoleread() if a whole line (or end-of-file)
                // has arrived.
                cons.w = cons.e;
                wakeup(&cons.r);
            }
        }
        break;
    }

    release(&cons.lock);
}

void
consoleinit(void)
{
    initlock(&cons.lock, "cons");

    uartinit();

    // connect read and write system calls
    // to consoleread and consolewrite.
    devsw[CONSOLE].read = consoleread;
    devsw[CONSOLE].write = consolewrite;
    devsw[CONSOLE].ioctl = consoleioctl;
    set_default_termios(&termios);
}
