/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#include "kernel/types.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/fcntl.h"
#include "kernel/adc.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd, ret;

  fd = open("devadc", O_RDWR);
  if (fd < 0) {
    mknod("devadc", ADC, 0);
    fd = open("devadc", O_RDWR);
    if (fd < 0) {
      printf("failed to open devadc\n");
      return -1;
    }
  }

  /* adc1: chip 0, chan 1 */
  ret = ioctl(fd, ADC_IOCTL_GET, (void *)(1 << ADC_CHAN_SFT));
  if (ret >= 0)
    printf("adc raw: %d\n", ret);
  else
    printf("ioctl error: %d\n", ret);

  close(fd);

  return 0;
}
