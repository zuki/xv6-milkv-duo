/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2011, Marvell Semiconductor Inc.
 * Lei Wen <leiwen@marvell.com>
 *
 * Back ported to the 8xx platform (from the 8260 platform) by
 * Murray.Jensen@cmst.csiro.au, 27-Jan-01.
 */
#ifndef INC_SDHCI_H
#define INC_SDHCI_H

#include <common/types.h>
#include <mmc.h>
#include <riscv-mmio.h>
#ifdef DUO256
#include <sdhci_cv181x_reg.h>
#else
#include <sdhci_cv180x_reg.h>
#endif

#define BIT(nr)     (1UL << (nr))

/*
 * 制御レジスタ
 */

#define SDHCI_DMA_ADDRESS   0x00

#define SDHCI_BLOCK_SIZE    0x04
#define  SDHCI_MAKE_BLKSZ(dma, blksz) (((dma & 0x7) << 12) | (blksz & 0xFFF))

#define SDHCI_BLOCK_COUNT   0x06

#define SDHCI_ARGUMENT      0x08

#define SDHCI_TRANSFER_MODE 0x0C
#define  SDHCI_TRNS_DMA         BIT(0)
#define  SDHCI_TRNS_BLK_CNT_EN  BIT(1)
#define  SDHCI_TRNS_ACMD12      BIT(2)
#define  SDHCI_TRNS_READ        BIT(4)
#define  SDHCI_TRNS_MULTI       BIT(5)

#define SDHCI_COMMAND       0x0E
#define  SDHCI_CMD_RESP_MASK    0x03
#define  SDHCI_CMD_CRC          0x08
#define  SDHCI_CMD_INDEX        0x10
#define  SDHCI_CMD_DATA         0x20
#define  SDHCI_CMD_ABORTCMD     0xC0

#define  SDHCI_CMD_RESP_NONE        0x00
#define  SDHCI_CMD_RESP_LONG        0x01
#define  SDHCI_CMD_RESP_SHORT       0x02
#define  SDHCI_CMD_RESP_SHORT_BUSY  0x03

#define SDHCI_MAKE_CMD(c, f)    (((c & 0xff) << 8) | (f & 0xff))
#define SDHCI_GET_CMD(c)        ((c>>8) & 0x3f)

#define SDHCI_RESPONSE      0x10

#define SDHCI_BUFFER        0x20

#define SDHCI_PRESENT_STATE 0x24
#define  SDHCI_CMD_INHIBIT      BIT(0)
#define  SDHCI_DATA_INHIBIT     BIT(1)
#define  SDHCI_DOING_WRITE      BIT(8)
#define  SDHCI_DOING_READ       BIT(9)
#define  SDHCI_SPACE_AVAILABLE  BIT(10)
#define  SDHCI_DATA_AVAILABLE   BIT(11)
#define  SDHCI_CARD_PRESENT     BIT(16)
#define  SDHCI_CARD_STATE_STABLE        BIT(17)
#define  SDHCI_CARD_DETECT_PIN_LEVEL    BIT(18)
#define  SDHCI_WRITE_PROTECT    BIT(19)

#define SDHCI_HOST_CONTROL  0x28
#define  SDHCI_CTRL_LED         BIT(0)
#define  SDHCI_CTRL_4BITBUS     BIT(1)
#define  SDHCI_CTRL_HISPD       BIT(2)
#define  SDHCI_CTRL_DMA_MASK    0x18
#define   SDHCI_CTRL_SDMA           0x00
#define   SDHCI_CTRL_ADMA1          0x08
#define   SDHCI_CTRL_ADMA32         0x10
#define   SDHCI_CTRL_ADMA64         0x18
#define  SDHCI_CTRL_8BITBUS     BIT(5)
#define  SDHCI_CTRL_CD_TEST_INS BIT(6)
#define  SDHCI_CTRL_CD_TEST     BIT(7)

#define SDHCI_POWER_CONTROL 0x29
#define  SDHCI_POWER_ON         0x01
#define  SDHCI_POWER_180        0x0A
#define  SDHCI_POWER_300        0x0C
#define  SDHCI_POWER_330        0x0E

#define SDHCI_BLOCK_GAP_CONTROL 0x2A

#define SDHCI_WAKE_UP_CONTROL   0x2B
#define  SDHCI_WAKE_ON_INT      BIT(0)
#define  SDHCI_WAKE_ON_INSERT   BIT(1)
#define  SDHCI_WAKE_ON_REMOVE   BIT(2)

#define SDHCI_CLOCK_CONTROL 0x2C
#define  SDHCI_DIVIDER_SHIFT    8
#define  SDHCI_DIVIDER_HI_SHIFT 6
#define  SDHCI_DIV_MASK         0xFF
#define  SDHCI_DIV_MASK_LEN     8
#define  SDHCI_DIV_HI_MASK      0x300
#define  SDHCI_PROG_CLOCK_MODE  BIT(5)
#define  SDHCI_CLOCK_CARD_EN    BIT(2)
#define  SDHCI_CLOCK_INT_STABLE BIT(1)
#define  SDHCI_CLOCK_INT_EN     BIT(0)

#define SDHCI_TIMEOUT_CONTROL   0x2E

#define SDHCI_SOFTWARE_RESET    0x2F
#define  SDHCI_RESET_ALL            0x01
#define  SDHCI_RESET_CMD            0x02
#define  SDHCI_RESET_DATA           0x04

#define SDHCI_INT_STATUS        0x30
#define SDHCI_ERR_INT_STATUS    0x32
#define SDHCI_INT_ENABLE        0x34
#define SDHCI_ERR_INT_STATUS_EN 0x36
#define SDHCI_SIGNAL_ENABLE     0x38
#define  SDHCI_INT_RESPONSE         BIT(0)
#define  SDHCI_INT_DATA_END         BIT(1)
#define  SDHCI_INT_DMA_END          BIT(3)
#define  SDHCI_INT_SPACE_AVAIL      BIT(4)
#define  SDHCI_INT_DATA_AVAIL       BIT(5)
#define  SDHCI_INT_CARD_INSERT      BIT(6)
#define  SDHCI_INT_CARD_REMOVE      BIT(7)
#define  SDHCI_INT_CARD_INT         BIT(8)
#define  SDHCI_INT_ERROR            BIT(15)
#define  SDHCI_INT_TIMEOUT          BIT(16)
#define  SDHCI_INT_CRC              BIT(17)
#define  SDHCI_INT_END_BIT          BIT(18)
#define  SDHCI_INT_INDEX            BIT(19)
#define  SDHCI_INT_DATA_TIMEOUT     BIT(20)
#define  SDHCI_INT_DATA_CRC         BIT(21)
#define  SDHCI_INT_DATA_END_BIT     BIT(22)
#define  SDHCI_INT_BUS_POWER        BIT(23)
#define  SDHCI_INT_ACMD12ERR        BIT(24)
#define  SDHCI_INT_ADMA_ERROR       BIT(25)

#define  SDHCI_INT_NORMAL_MASK  0x00007FFF
#define  SDHCI_INT_ERROR_MASK   0xFFFF8000

#define  SDHCI_INT_CMD_MASK     (SDHCI_INT_RESPONSE | SDHCI_INT_TIMEOUT | \
            SDHCI_INT_CRC | SDHCI_INT_END_BIT | SDHCI_INT_INDEX)
#define  SDHCI_INT_DATA_MASK    (SDHCI_INT_DATA_END | SDHCI_INT_DMA_END | \
            SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL | \
            SDHCI_INT_DATA_TIMEOUT | SDHCI_INT_DATA_CRC | \
            SDHCI_INT_DATA_END_BIT | SDHCI_INT_ADMA_ERROR)
#define SDHCI_INT_ALL_MASK      ((unsigned int)-1)

#define SDHCI_ACMD12_ERR        0x3C

#define SDHCI_HOST_CONTROL2     0x3E
#define SDHCI_HOST_VER4_ENABLE      BIT(12)
#define SDHCI_HOST_ADDRESSING       BIT(13)
#define SDHCI_HOST_ADMA2_LEN_MODE   BIT(10)
#define  SDHCI_CTRL_UHS_MASK        0x0007
#define  SDHCI_CTRL_UHS_SDR12       0x0000
#define  SDHCI_CTRL_UHS_SDR25       0x0001
#define  SDHCI_CTRL_UHS_SDR50       0x0002
#define  SDHCI_CTRL_UHS_SDR104      0x0003
#define  SDHCI_CTRL_UHS_DDR50       0x0004
#define  SDHCI_CTRL_HS400           0x0005 /* Non-standard */
#define  SDHCI_CTRL_VDD_180         0x0008
#define  SDHCI_CTRL_DRV_TYPE_MASK   0x0030
#define  SDHCI_CTRL_DRV_TYPE_B      0x0000
#define  SDHCI_CTRL_DRV_TYPE_A      0x0010
#define  SDHCI_CTRL_DRV_TYPE_C      0x0020
#define  SDHCI_CTRL_DRV_TYPE_D      0x0030
#define  SDHCI_CTRL_EXEC_TUNING     0x0040
#define  SDHCI_CTRL_TUNED_CLK       0x0080
#define  SDHCI_CTRL_PRESET_VAL_ENABLE   0x8000

#define SDHCI_CAPABILITIES      0x40
#define  SDHCI_TIMEOUT_CLK_MASK     0x0000003F
#define  SDHCI_TIMEOUT_CLK_SHIFT    0
#define  SDHCI_TIMEOUT_CLK_UNIT     0x00000080
#define  SDHCI_CLOCK_BASE_MASK      0x00003F00
#define  SDHCI_CLOCK_V3_BASE_MASK   0x0000FF00
#define  SDHCI_CLOCK_BASE_SHIFT     8
#define  SDHCI_MAX_BLOCK_MASK       0x00030000
#define  SDHCI_MAX_BLOCK_SHIFT      16
#define  SDHCI_CAN_DO_8BIT          BIT(18)
#define  SDHCI_CAN_DO_ADMA2         BIT(19)
#define  SDHCI_CAN_DO_ADMA1         BIT(20)
#define  SDHCI_CAN_DO_HISPD         BIT(21)
#define  SDHCI_CAN_DO_SDMA          BIT(22)
#define  SDHCI_CAN_VDD_330          BIT(24)
#define  SDHCI_CAN_VDD_300          BIT(25)
#define  SDHCI_CAN_VDD_180          BIT(26)
#define  SDHCI_CAN_64BIT            BIT(28)

#define SDHCI_CAPABILITIES_1    0x44
#define  SDHCI_SUPPORT_SDR50        0x00000001
#define  SDHCI_SUPPORT_SDR104       0x00000002
#define  SDHCI_SUPPORT_DDR50        0x00000004
#define  SDHCI_USE_SDR50_TUNING     0x00002000

#define  SDHCI_CLOCK_MUL_MASK       0x00FF0000
#define  SDHCI_CLOCK_MUL_SHIFT      16

#define SDHCI_MAX_CURRENT       0x48

/* 4C-4F reserved for more max current */

#define SDHCI_SET_ACMD12_ERROR  0x50
#define SDHCI_SET_INT_ERROR     0x52

#define SDHCI_ADMA_ERROR        0x54

/* 55-57 reserved */

#define SDHCI_ADMA_ADDRESS      0x58
#define SDHCI_ADMA_ADDRESS_HI   0x5c

/* 60-FB reserved */

#define SDHCI_SLOT_INT_STATUS   0xFC

#define SDHCI_HOST_VERSION      0xFE
#define  SDHCI_VENDOR_VER_MASK      0xFF00
#define  SDHCI_VENDOR_VER_SHIFT     8
#define  SDHCI_SPEC_VER_MASK        0x00FF
#define  SDHCI_SPEC_VER_SHIFT       0
#define   SDHCI_SPEC_100                0
#define   SDHCI_SPEC_200                1
#define   SDHCI_SPEC_300                2

#define SDHCI_GET_VERSION(x)    (x->version & SDHCI_SPEC_VER_MASK)

/* 制御レジスタ定義はここまで */

#define SDHCI_MAX_DIV_SPEC_200  256
#define SDHCI_MAX_DIV_SPEC_300  2046

/*
 * quirks
 */
#define SDHCI_QUIRK_32BIT_DMA_ADDR  (1 << 0)
#define SDHCI_QUIRK_REG32_RW        (1 << 1)
#define SDHCI_QUIRK_BROKEN_R1B      (1 << 2)
#define SDHCI_QUIRK_NO_HISPD_BIT    (1 << 3)
#define SDHCI_QUIRK_BROKEN_VOLTAGE  (1 << 4)

/*
 * SDHCI_QUIRK_BROKEN_HISPD_MODE
 * the hardware cannot operate correctly in high-speed mode,
 * this quirk forces the sdhci host-controller to non high-speed mode
 */
#define SDHCI_QUIRK_BROKEN_HISPD_MODE   BIT(5)
#define SDHCI_QUIRK_WAIT_SEND_CMD       (1 << 6)
#define SDHCI_QUIRK_USE_WIDE8           (1 << 8)
#define SDHCI_QUIRK_NO_1_8_V            (1 << 9)

/* Host SDMAのバッファ境界. 有効な値は4Kから512Kの2のべき乗. */
#define SDHCI_DEFAULT_BOUNDARY_SIZE     (512 * 1024)
#define SDHCI_DEFAULT_BOUNDARY_ARG      (7)

struct sdhci_host {
    const char *name;
    void *ioaddr;
    unsigned int quirks;
    unsigned int host_caps;
    unsigned int version;
    unsigned int max_clk;   /* Maximum Base Clock frequency */
    unsigned int clk_mul;   /* Clock Multiplier value */
    unsigned int clock;
    struct mmc *mmc;
    int index;              /* MMC_TYPE_SD */
    int bus_width;
    unsigned int    voltages;
    struct mmc_config cfg;
    void *align_buffer;
    bool force_align_buffer;
    dma_addr_t start_addr;
    int flags;
#define USE_SDMA    (0x1 << 0)
#define USE_ADMA    (0x1 << 1)
#define USE_ADMA64  (0x1 << 2)
#define USE_DMA     (USE_SDMA | USE_ADMA | USE_ADMA64)
};

static inline void sdhci_writed(struct sdhci_host *host, uint64_t val, int reg)
{
    mmio_write_64((unsigned long)(host->ioaddr + reg), val);
}

static inline void sdhci_writel(struct sdhci_host *host, uint32_t val, int reg)
{
    mmio_write_32((unsigned long)(host->ioaddr + reg), val);
}

static inline void sdhci_writew(struct sdhci_host *host, uint16_t val, int reg)
{
    mmio_write_16((unsigned long)(host->ioaddr + reg), val);
}

static inline void sdhci_writeb(struct sdhci_host *host, uint8_t val, int reg)
{
    mmio_write_8((unsigned long)(host->ioaddr + reg), val);
}

static inline uint64_t sdhci_readd(struct sdhci_host *host, int reg)
{
    return mmio_read_64((unsigned long)(host->ioaddr + reg));
}

static inline uint32_t sdhci_readl(struct sdhci_host *host, int reg)
{
    return mmio_read_32((unsigned long)(host->ioaddr + reg));
}

static inline uint16_t sdhci_readw(struct sdhci_host *host, int reg)
{
    return mmio_read_16((unsigned long)(host->ioaddr + reg));
}

static inline uint8_t sdhci_readb(struct sdhci_host *host, int reg)
{
    return mmio_read_8((unsigned long)(host->ioaddr + reg));
}

int sdhci_send_command(struct mmc *mmc, struct mmc_cmd *cmd,
            struct mmc_data *data);
int sdhci_set_clock(struct mmc *mmc, unsigned int clock);
int sdhci_set_ios(struct mmc *mmc);
int sdhci_init(struct mmc *mmc);
int sdhci_probe(struct mmc *mmc);
int sdhci_get_cd(struct mmc *mmc);
int sdhci_setup_cfg(struct mmc_config *cfg, struct sdhci_host *host,
            uint32_t f_max, uint32_t f_min);
void sdhci_set_uhs_timing(struct sdhci_host *host);
void sdhci_set_voltage(struct mmc *mmc);
int sdhci_card_busy(struct mmc *mmc, int state, int timeout_us);
int sdhci_execute_tuning(struct mmc *mmc, unsigned int opcode);

int cvi_general_execute_tuning(struct mmc *mmc, uint8_t opcode);
int cvi_get_cd(struct sdhci_host *host);
int cvi_sdhci_probe(struct mmc *mmc);
void cvi_general_reset(struct sdhci_host *host, uint8_t mask);
void cvi_sd_voltage_switch(struct mmc *mmc);


/* Export the operations to drivers */
int sdhci_probe(struct mmc *mmc);

#endif /* INC_SDHCI_H */
