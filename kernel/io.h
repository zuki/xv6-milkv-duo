/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#ifndef _IO_H__
#define _IO_H_

#include "types.h"

static inline void write8(unsigned long addr, uint8 value)
{
  *(volatile uint8 *)addr = value;
}

static inline uint8 read8(unsigned long addr)
{
  return *(volatile uint8 *)addr;
}

static inline void write16(unsigned long addr, uint16 value)
{
  *(volatile uint16 *)addr = value;
}

static inline uint16 read16(unsigned long addr)
{
  return *(volatile uint16 *)addr;
}

static inline void write32(unsigned long addr, uint32 value)
{
  *(volatile uint32 *)addr = value;
}

static inline uint32 read32(unsigned long addr)
{
  return *(volatile uint32 *)addr;
}

static inline void write64(unsigned long addr, uint64 value)
{
  *(volatile uint64 *)addr = value;
}

static inline uint64 read64(unsigned long addr)
{
  return *(volatile uint64 *)addr;
}

#endif
