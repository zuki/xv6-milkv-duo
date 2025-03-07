/* Copyright (C) 2023 Jisheng Zhang <jszhang@kernel.org> */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#undef  DUO
#define DUO256

#define LOG_DEBUG

/* T-Head社のRISCV extensionを使用 */
#define THEAD_PTE

/* 1_8_vの設定を行わない */
/*  - test中は立ち上がりが少し遅くなるので 1 = true  */
/*  - 本番はSDカードのread/writeが早くなるので 0 = flase */
#define NO_1_8_V    1

/* maximum number of CPUs */
#define NCPU        1

#define UART0 0x04140000UL
#define UART0_PHY UART0
#define UART0_IRQ 44
#define REG_SHIFT 2
#define UART_CLK 25000000UL
#define BAUD_RATE 115200UL

#define PLIC 0x70000000UL

/* core local interruptor (CLINT), which contains the timer. */
#define CLINT 0x74000000L

#define SD0_DRIVER
#define SD0 0x04310000
#define SD0_IRQ     36

#define INTERVAL     250000UL
#define US_INTERVAL  25UL


#endif
