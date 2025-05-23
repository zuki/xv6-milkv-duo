#include <common/types.h>
#include <common/param.h>
#include <common/memlayout.h>
#include <common/riscv.h>
#include "defs.h"
#include "config.h"

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void
plicinit(void)
{
  *(uint32_t *)PLIC_CTRL = 1;
  // set desired IRQ priorities non-zero (otherwise disabled).
  *(uint32_t *)(PLIC + UART0_IRQ*4) = 1;
  *(uint32_t *)(PLIC + SD0_IRQ*4) = 1;
}

void
plicinithart(void)
{
  int hart = cpuid();

  // set enable bits for this hart's S-mode
  // for the uart.
  //*(uint32_t *)(PLIC_SENABLE(hart) + (UART0_IRQ / 32) * 4) = (1 << (UART0_IRQ % 32));
  *(uint32_t *)PLIC_SENABLE1(hart) = (1 << (UART0_IRQ - 32)) | (1 << (SD0_IRQ - 32));
  // set this hart's S-mode priority threshold to 0.
  *(uint32_t *)PLIC_SPRIORITY(hart) = 0;
}

// ask the PLIC what interrupt we should serve.
int
plic_claim(void)
{
  int hart = cpuid();
  int irq = *(uint32_t *)PLIC_SCLAIM(hart);
  return irq;
}

// tell the PLIC we've served this IRQ.
void
plic_complete(int irq)
{
  int hart = cpuid();
  *(uint32_t *)PLIC_SCLAIM(hart) = irq;
}
