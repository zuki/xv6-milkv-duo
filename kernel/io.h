/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#ifndef _IO_H__
#define _IO_H_

#include "types.h"

static inline void write8(unsigned long addr, uint8_t value)
{
  *(volatile uint8_t *)addr = value;
}

static inline uint8_t read8(unsigned long addr)
{
  return *(volatile uint8_t *)addr;
}

static inline void write16(unsigned long addr, uint16_t value)
{
  *(volatile uint16_t *)addr = value;
}

static inline uint16_t read16(unsigned long addr)
{
  return *(volatile uint16_t *)addr;
}

static inline void write32(unsigned long addr, uint32_t value)
{
  *(volatile uint32_t *)addr = value;
}

static inline uint32_t read32(unsigned long addr)
{
  return *(volatile uint32_t *)addr;
}

static inline void write64(unsigned long addr, uint64_t value)
{
  *(volatile uint64_t *)addr = value;
}

static inline uint64_t read64(unsigned long addr)
{
  return *(volatile uint64_t *)addr;
}

#endif
