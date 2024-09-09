/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#ifndef _SPI_H_
#define _SPI_H_

#define SPI_IOCTL_SET_SPEED 0
#define SPI_IOCTL_SET_MODE 1
#define SPI_IOCTL_TRANSFER 2

struct spi_msg {
  int len;
  unsigned long rx_buf;
  unsigned long tx_buf;
};

#endif
