/*
 * Copyright (c) 2013-2014, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_MMIO_H
#define RISCV_MMIO_H

#include <common/types.h>
#include <riscv-barrier.h>

static inline void mmio_write_8(uintptr_t addr, uint8_t value)
{
    wmb();
    *(volatile uint8_t*)addr = value;
}

static inline uint8_t mmio_read_8(uintptr_t addr)
{
    uint8_t val = *(volatile uint8_t*)addr;
    rmb();
    return val;
}

static inline void mmio_write_16(uintptr_t addr, uint16_t value)
{
    wmb();
    *(volatile uint16_t*)addr = value;
}

static inline uint16_t mmio_read_16(uintptr_t addr)
{
    uint16_t val = *(volatile uint16_t*)addr;
    rmb();
    return val;
}

static inline void mmio_write_32(uintptr_t addr, uint32_t value)
{
    wmb();
    *(volatile uint32_t*)addr = value;
}

static inline uint32_t mmio_read_32(uintptr_t addr)
{
    uint32_t val = *(volatile uint32_t*)addr;
    rmb();
    return val;
}

static inline void mmio_write_64(uintptr_t addr, uint64_t value)
{
    wmb();
    *(volatile uint64_t*)addr = value;
}

static inline uint64_t mmio_read_64(uintptr_t addr)
{
    uint64_t val = *(volatile uint64_t*)addr;
    rmb();
    return val;
}

// addrのレジスタのclearビットをクリアする
static inline void mmio_clrbits_32(uintptr_t addr, uint32_t clear)
{
    mmio_write_32(addr, mmio_read_32(addr) & ~clear);
}

// addrのレジスタのsetビットをセットする
static inline void mmio_setbits_32(uintptr_t addr, uint32_t set)
{
    mmio_write_32(addr, mmio_read_32(addr) | set);
}

// addrのレジスタのclearビットをクリアしてsetビットをセットする
static inline void mmio_clrsetbits_32(uintptr_t addr, uint32_t clear,
    uint32_t set)
{
    mmio_write_32(addr, (mmio_read_32(addr) & ~clear) | set);
}

#endif /* RISCV_MMIO_H */
