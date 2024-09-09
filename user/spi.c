/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#include "kernel/types.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/fcntl.h"
#include "kernel/spi.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd, i, ret;
  struct spi_msg msg;
  unsigned char tx[32], rx[32];

  fd = open("devspi", O_RDWR);
  if (fd < 0) {
    mknod("devspi", SPI, 0);
    fd = open("devspi", O_RDWR);
    if (fd < 0) {
      printf("failed to open devspi\n");
      return -1;
    }
  }

  ret = ioctl(fd, SPI_IOCTL_SET_SPEED, (void *)500000);
  if (ret < 0) {
    printf("SPI_IOCTL_SET_SPEED failed. Check your spi slave device\n");
    return ret;
  }

  msg.len = sizeof(tx);
  msg.tx_buf = (unsigned long)tx;
  msg.rx_buf = (unsigned long)rx;
  for (i = 0; i < sizeof(tx); i++)
	  tx[i] = i;
  ret = ioctl(fd, SPI_IOCTL_TRANSFER, &msg);
  if (ret < 0) {
    printf("SPI_IOCTL_TRANSFER failed. Check your spi slave device\n");
    return ret;
  }

  printf("RX:\n");
  for (i = 0; i < sizeof(rx); i++)
	  printf("%d ", rx[i]);

  close(fd);

  return 0;
}
