/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#ifndef _PWM_H_
#define _PWM_H_

#define PWM_IOCTL_CONFIG	0

struct pwm_config {
 /* period/duty in ns */
  unsigned long period;
  unsigned long duty;
  int chip;
  int chan;
  /* 0 -> high during duty period, 1 -> low for duty period */
  unsigned char polarity;
  /* 0 -> disable, 1 -> enable */
  unsigned char enable;
};

#endif
