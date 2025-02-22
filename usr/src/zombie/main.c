// Create a zombie process that
// must be reparented at exit.

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>

int
main(void)
{
    if (fork() > 0)
        sleep(5);  // Let child exit before parent.
    tcdrain(1);
    _exit(0);
    //return 0;
}
