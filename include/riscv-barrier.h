/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 2013 Regents of the University of California
 * Copyright (C) 2017 SiFive
 *
 * Taken from Linux arch/riscv/include/asm/barrier.h, which is based on
 * arch/arm/include/asm/barrier.h
 */

#ifndef INC_RISCV_BARRIER_H
#define INC_RISCV_BARRIER_H

#ifndef __ASSEMBLY__

#define nop()   __asm__ __volatile__ ("nop")

#define RISCV_FENCE(p, s) \
    __asm__ __volatile__ ("fence " #p "," #s : : : "memory")

/* These barriers need to enforce ordering on both devices or memory. */
#define mb()    RISCV_FENCE(iorw,iorw)
#define rmb()   RISCV_FENCE(ir,ir)
#define wmb()   RISCV_FENCE(ow,ow)

/* These barriers do not need to enforce ordering on devices, just memory. */
#define __smp_mb()      RISCV_FENCE(rw,rw)
#define __smp_rmb()     RISCV_FENCE(r,r)
#define __smp_wmb()     RISCV_FENCE(w,w)

#define __smp_store_release(p, v)       \
do {                                    \
    compiletime_assert_atomic_type(*p); \
    RISCV_FENCE(rw,w);                  \
    WRITE_ONCE(*p, v);                  \
} while (0)

#define __smp_load_acquire(p)           \
({                                      \
    typeof(*p) ___p1 = READ_ONCE(*p);   \
    compiletime_assert_atomic_type(*p); \
    RISCV_FENCE(r,rw);                  \
    ___p1;                              \
})

/*
 * This is a very specific barrier: it's currently only used in two places in
 * the kernel, both in the scheduler.  See include/linux/spinlock.h for the two
 * orderings it guarantees, but the "critical section is RCsc" guarantee
 * mandates a barrier on RISC-V.  The sequence looks like:
 *
 *    lr.aq lock
 *    sc    lock <= LOCKED
 *    smp_mb__after_spinlock()
 *    // critical section
 *    lr    lock
 *    sc.rl lock <= UNLOCKED
 *
 * The AQ/RL pair provides a RCpc critical section, but there's not really any
 * way we can take advantage of that here because the ordering is only enforced
 * on that one lock.  Thus, we're just doing a full fence.
 */
#define smp_mb__after_spinlock()    RISCV_FENCE(rw,rw)

#endif /* __ASSEMBLY__ */

#endif /* INC_RISCV_BARRIER_H */
