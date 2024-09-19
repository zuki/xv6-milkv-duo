/* Copyright (C) 2023 Jisheng Zhang <jszhang@kernel.org> */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

/* config for CV1800b */
#define CV180X
#define LOG_DEBUG

#define THEAD_PTE

/* maximum number of CPUs */
#define NCPU          1

#define UART0 0x04140000UL
#define UART0_PHY UART0
#define UART0_IRQ 44
#define REG_SHIFT 2
#define UART_CLK 25000000UL
#define BAUD_RATE 115200UL

#define PLIC 0x70000000UL

/* core local interruptor (CLINT), which contains the timer. */
#define CLINT 0x74000000L

#define GPIO_DRIVER
#define GPIO_NUM 4
/* #define GPIO0 0x3020000 */
/* #define GPIO1 0x3021000 */
#define GPIO2 0x3022000
/* #define GPIO3 0x3023000 */

#define PWM_DRIVER
#define PWM_NUM 4
/* #define PWM0 0x3060000 */
#define PWM1 0x3061000
/* #define PWM2 0x3062000 */
/* #define PWM3 0x3063000 */
#define PWM_CLKSRC_FREQ 100000000UL

#define ADC_DRIVER
#define ADC_NUM 1
#define ADC0 0x30f0000

#define I2C_DRIVER
#define I2C0 0x4000000

#define SPI_DRIVER
#define SPI0 0x41a0000
#define SPI0_MAX_FREQ 500000
#define SPI0_CLK_FREQ 100000000UL

#define SD0_DRIVER
#define SD0 0x04310000
#define SD0_IRQ     36

#define INTERVAL     250000UL
#define US_INTERVAL  25UL


#endif
