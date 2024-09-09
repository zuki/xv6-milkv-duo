/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#include "kernel/types.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/fcntl.h"
#include "kernel/pwm.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd;
  struct pwm_config conf;

  if (argc != 3) {
    printf("./pwm period duty\n");
    printf("Example set period as 1000000ns and duty as 600000ns: ./pwm 1000000 600000\n");
    return -1;
  }

  fd = open("devpwm", O_RDWR);
  if (fd < 0) {
    mknod("devpwm", PWM, 0);
    fd = open("devpwm", O_RDWR);
    if (fd < 0) {
      printf("failed to open devpwm\n");
      return -1;
    }
  }

  /* PWM 6 */
  conf.chip = 1;
  conf.chan = 2;
  conf.enable = 1;
  conf.polarity = 0;
  conf.period = atoi(argv[1]);
  conf.duty = atoi(argv[2]);
  ioctl(fd, PWM_IOCTL_CONFIG, &conf);

  close(fd);

  return 0;
}
