/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#include "kernel/types.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/fcntl.h"
#include "kernel/i2c.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd, ret;
  struct i2c_msg msg;
  unsigned char val;

  if (argc != 3 && argc != 4) {
    printf("Usage:\n");
    printf("Read: ./i2c addr reg\n");
    printf("Write: ./i2c addr reg val\n");
    return -1;
  }

  fd = open("devi2c", O_RDWR);
  if (fd < 0) {
    mknod("devi2c", I2C, 0);
    fd = open("devi2c", O_RDWR);
    if (fd < 0) {
      printf("failed to open devi2c\n");
      return -1;
    }
  }

  msg.addr = strtoul(argv[1], NULL, 0);
  msg.buf = &val;
  msg.reg = strtoul(argv[2], NULL, 0);
  msg.len = 1;

  if (argc == 3) {
    ret = ioctl(fd, I2C_IOCTL_RD, &msg);
    if (ret < 0) {
      printf("I2C_IOCTL_RD failed. Check your i2c slave device\n");
      return ret;
    }

    printf("0x%x\n", val);
  } else {
    val = strtoul(argv[3], NULL, 0);
    ret = ioctl(fd, I2C_IOCTL_WR, &msg);
    if (ret < 0) {
      printf("I2C_IOCTL_WR failed. Check your i2c slave device\n");
      return ret;
    }
  }

  close(fd);

  return 0;
}
