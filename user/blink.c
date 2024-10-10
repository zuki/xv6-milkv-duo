/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#include <common/types.h>
#include <common/spinlock.h>
#include <common/sleeplock.h>
#include <common/fs.h>
#include <common/file.h>
#include <common/fcntl.h>
#include <common/gpio.h>
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd, i, loops;
  struct gpio_config conf;

  if (argc != 2) {
    printf("./blink LOOPS_CNT\n");
    return -1;
  }

  fd = open("devgpio", O_RDWR);
  if (fd < 0) {
    mknod("devgpio", GPIO, 0);
    fd = open("devgpio", O_RDWR);
    if (fd < 0) {
      printf("failed to open devgpio\n");
      return -1;
    }
  }

  conf.pin = (2 << 8) | 24; //port 2, gpio 24
  conf.mode = GPIO_OUT;
  ioctl(fd, GPIO_IOCTL_CONFIG, &conf);

  loops = atoi(argv[1]);
  for (i = 0; i < loops; i++) {
    ioctl(fd, GPIO_IOCTL_SET, (void *)((2 << 8) | 24));
    sleep(100);
    ioctl(fd, GPIO_IOCTL_SET, (void *)((1 << 16 | (2 << 8) | 24)));
    sleep(100);
  }

  close(fd);

  return 0;
}
