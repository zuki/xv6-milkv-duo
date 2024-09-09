/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */


#include "config.h"
#include "gpio.h"
#include "io.h"
#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "proc.h"

#ifdef GPIO_DRIVER

#define PORT_DR		0x00
#define PORT_DDR	0x04

struct gpioport {
  unsigned long base;
  struct spinlock lock;
};

struct gpio {
  struct gpioport port[GPIO_NUM];
} gpio;

static int gpio_config(struct gpio_config *argp)
{
  int ret, pin, port;
  uint32 val;
  struct gpio_config conf;
  struct proc *p = myproc();

  ret = copyin(p->pagetable, (char *)&conf, (uint64)argp, sizeof(*argp));
  if (ret < 0)
    return ret;

  pin = conf.pin & 0xff;
  port = (conf.pin >> 8) & 0xff;
  if (port < 0 || port > GPIO_NUM - 1 || !gpio.port[port].base)
    return -1;
  if (pin < 0 || pin > 31)
    return -1;

  acquire(&gpio.port[port].lock);
  val = read32(gpio.port[port].base + PORT_DDR);
  if (conf.mode & GPIO_IN) {
    val &= ~(1 << pin);
  } else if (conf.mode & GPIO_OUT) {
    val |= (1 << pin);
  } else {
    ret = -1;
    goto err;
  }
  write32(gpio.port[port].base + PORT_DDR, val);
err:
  release(&gpio.port[port].lock);

  return ret;
}

static int gpio_set(unsigned long arg)
{
  int pin, port;
  uint32 val;

  pin = arg & 0xff;
  port = (arg >> GPIO_PORT_SFT) & 0xff;
  if (port < 0 || port > GPIO_NUM - 1 || !gpio.port[port].base)
    return -1;
  if (pin < 0 || pin > 31)
    return -1;

  acquire(&gpio.port[port].lock);
  val = read32(gpio.port[port].base + PORT_DR);
  if (arg & (1 << GPIO_VAL_SFT))
    val |= (1 << pin);
  else
    val &= ~(1 << pin);
  write32(gpio.port[port].base + PORT_DR, val);
  release(&gpio.port[port].lock);

  return 0;
}

static int gpio_get(unsigned long arg)
{
  int pin, port;
  uint32 val;

  pin = arg & 0xff;
  port = (arg >> GPIO_PORT_SFT) & 0xff;
  if (port < 0 || port > GPIO_NUM - 1 || !gpio.port[port].base)
    return -1;
  if (pin < 0 || pin > 31)
    return -1;

  val = read32(gpio.port[port].base + PORT_DR);
  return (val >> pin) & 1;
}

static int gpio_ioctl(int user_dst, unsigned long req, void *argp)
{
  switch(req) {
  case GPIO_IOCTL_CONFIG:
    return gpio_config(argp);
  case GPIO_IOCTL_SET:
    return gpio_set((unsigned long)argp);
  case GPIO_IOCTL_GET:
    return gpio_get((unsigned long)argp);
  default:
    return -1;
  }
}

void
gpioinit(void)
{
#ifdef GPIO0
  initlock(&gpio.port[0].lock, "gpio_port0");
  gpio.port[0].base = GPIO0;
#endif
#ifdef GPIO1
  initlock(&gpio.port[1].lock, "gpio_port1");
  gpio.port[1].base = GPIO1;
#endif
#ifdef GPIO2
  initlock(&gpio.port[2].lock, "gpio_port2");
  gpio.port[2].base = GPIO2;
#endif
#ifdef GPIO3
  initlock(&gpio.port[3].lock, "gpio_port3");
  gpio.port[3].base = GPIO3;
#endif
  devsw[GPIO].ioctl = gpio_ioctl;
}

#endif
