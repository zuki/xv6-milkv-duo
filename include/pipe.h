#ifndef INC_PIPE_H
#define INC_PIPE_H

#include <common/types.h>
#include <linux/fcntl.h>
#include <spinlock.h>

#define PIPESIZE 512

#define PIPE2_FLAGS (O_CLOEXEC | O_DIRECT | O_NONBLOCK)

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint32_t nread;     // number of bytes read
  uint32_t nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};

#endif
