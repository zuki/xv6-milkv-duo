#ifndef INC_LINUX_POLL_H
#define INC_LINUX_POLL_H

#include <common/types.h>

#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#ifndef POLLWRNORM
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200
#endif
#ifndef POLLMSG
#define POLLMSG    0x400
#define POLLRDHUP  0x2000
#endif

/* for sys_poll */
struct pollfd {
    int fd;
    int16_t events;
    int16_t revents;
};

#endif
