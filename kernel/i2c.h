/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#ifndef _I2C_H_
#define _I2C_H_

#define I2C_IOCTL_RD	0
#define I2C_IOCTL_WR	1

struct i2c_msg {
  unsigned char addr;
  unsigned char reg;
  int len;
  unsigned char *buf;
};

#endif
