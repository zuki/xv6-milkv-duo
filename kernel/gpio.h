/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#ifndef _GPIO_H_
#define _GPIO_H_

#define GPIO_IN		(1 << 0)
#define GPIO_OUT	(1 << 1)
#define GPIO_HIGH	(1 << 2)

#define GPIO_PIN_SFT	0
#define GPIO_PORT_SFT	8
#define GPIO_VAL_SFT	16

#define GPIO_IOCTL_CONFIG	0
#define GPIO_IOCTL_GET		1
#define GPIO_IOCTL_SET		2

struct gpio_config {
  int pin;
  int mode;
};

#endif
