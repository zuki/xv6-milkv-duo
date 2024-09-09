/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#include "config.h"
#include "pwm.h"
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

#ifdef PWM_DRIVER

#define HLPERIOD0	0x00
#define PERIOD0		0x04
#define HLPERIOD(n)	(n * 8 + HLPERIOD0)
#define PERIOD(n)	(n * 8 + PERIOD0)
#define POLARITY	0x40
#define  POLARITY_SFT	0
#define  PWMMODE_SFT	8
#define PWMSTART	0x44
#define PWM_OE		0xd0

struct pwmchip {
  unsigned long base;
  struct spinlock lock;
};

struct pwm {
  struct pwmchip chip[PWM_NUM];
} pwm;

static int pwm_config(struct pwm_config *argp)
{
  int ret, chip, chan;
  uint32 val, duty, start;
  struct pwm_config conf;
  struct proc *p = myproc();

  ret = copyin(p->pagetable, (char *)&conf, (uint64)argp, sizeof(*argp));
  if (ret < 0)
    return ret;

  chip = conf.chip;
  chan = conf.chan;
  if (chip < 0 || chip > PWM_NUM - 1 || !pwm.chip[chip].base)
    return -1;
  if (chan < 0 || chan > 3)
    return -1;

  acquire(&pwm.chip[chip].lock);

  start = read32(pwm.chip[chip].base + PWMSTART);
  start &= ~(1 << chan);
  write32(pwm.chip[chip].base + PWMSTART, start);

  if (!conf.enable)
    goto out;

  val = read32(pwm.chip[chip].base + PWM_OE);
  val |= (1 << chan);
  write32(pwm.chip[chip].base + PWM_OE, val);

  val = 1000000000 / PWM_CLKSRC_FREQ;
  val = conf.period / val;
  if (val < 2 || val > ((1 << 30) - 1)) {
    ret = -1;
    goto out;
  }
  write32(pwm.chip[chip].base + PERIOD(chan), val);

  duty = 1000000000 / PWM_CLKSRC_FREQ;
  duty = conf.duty / duty;
  if (val < 1 || duty >= val) {
    ret = -1;
    goto out;
  }
  write32(pwm.chip[chip].base + HLPERIOD(chan), duty);

  val = 0;
  if (!conf.polarity)
    val |= (1 << (chan + POLARITY_SFT));
  write32(pwm.chip[chip].base + POLARITY, val);

  start |= (1 << chan);
  write32(pwm.chip[chip].base + PWMSTART, start);

out:
  release(&pwm.chip[chip].lock);

  return ret;
}

static int pwm_ioctl(int user_dst, unsigned long req, void *argp)
{
  switch(req) {
  case PWM_IOCTL_CONFIG:
    return pwm_config(argp);
  default:
    return -1;
  }
}

void
pwminit(void)
{
  initlock(&pwm.chip[0].lock, "pwm0");
  initlock(&pwm.chip[1].lock, "pwm1");
  initlock(&pwm.chip[2].lock, "pwm2");
  initlock(&pwm.chip[3].lock, "pwm3");
#ifdef PWM0
  pwm.chip[0].base = PWM0;
#endif
#ifdef PWM1
  pwm.chip[1].base = PWM1;
#endif
#ifdef PWM2
  pwm.chip[2].base = PWM2;
#endif
#ifdef PWM3
  pwm.chip[3].base = PWM3;
#endif
  devsw[PWM].ioctl = pwm_ioctl;
}

#endif
