//
/* emmc.cpp */
//
/* Provides an interface to the EMMC controller and commands for interacting */
/* with an sd card */
//
/* Copyright (C) 2013 by John Cronin <jncronin@tysos.org> */
//
/* Modified for Circle by R. Stange */
//
/* Permission is hereby granted, free of charge, to any person obtaining a copy */
/* of this software and associated documentation files (the "Software"), to deal */
/* in the Software without restriction, including without limitation the rights */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell */
/* copies of the Software, and to permit persons to whom the Software is */
/* furnished to do so, subject to the following conditions: */
//
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software. */
//
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN */
/* THE SOFTWARE. */
//
/* References: */
//
/* PLSS - SD Group Physical Layer Simplified Specification ver 3.00 */
/* HCSS - SD Group Host Controller Simplified Specification ver 3.00 */
//
/* Broadcom BCM2835 ARM Peripherals Guide */
//
#include "config.h"

#ifdef CV180X

#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "emmc.h"
#include "sdhci_cv180x.h"
#include "bitops.h"
#include "io.h"
#include "errno.h"
#include "printf.h"

const char *sd_versions[] = {
    "unknown",
    "1.0 or 1.01",
    "1.10",
    "2.00",
    "3.0x",
    "4.xx"
};

const char *err_irpts[] = {
    "CMD_TIMEOUT",
    "CMD_CRC",
    "CMD_END_BIT",
    "CMD_INDEX",
    "DATA_TIMEOUT",
    "DATA_CRC",
    "DATA_END_BIT",
    "CURRENT_LIMIT",
    "AUTO_CMD12",
    "ADMA",
    "TUNING",
    "RSVD"
};

const uint32_t sd_commands[] = {
    SD_CMD_INDEX(0),
    SD_CMD_RESERVED(1),
    SD_CMD_INDEX(2) | SD_RESP_R2,
    SD_CMD_INDEX(3) | SD_RESP_R6,
    SD_CMD_INDEX(4),
    SD_CMD_INDEX(5) | SD_RESP_R4,
    SD_CMD_INDEX(6) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(7) | SD_RESP_R1b,
    SD_CMD_INDEX(8) | SD_RESP_R7,
    SD_CMD_INDEX(9) | SD_RESP_R2,
    SD_CMD_INDEX(10) | SD_RESP_R2,
    SD_CMD_INDEX(11) | SD_RESP_R1,
    SD_CMD_INDEX(12) | SD_RESP_R1b | SD_CMD_TYPE_ABORT,
    SD_CMD_INDEX(13) | SD_RESP_R1,
    SD_CMD_RESERVED(14),
    SD_CMD_INDEX(15),
    SD_CMD_INDEX(16) | SD_RESP_R1,
    SD_CMD_INDEX(17) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(18) | SD_RESP_R1 | SD_DATA_READ | SD_CMD_MULTI_BLOCK | SD_CMD_BLKCNT_EN | SD_CMD_AUTO_CMD_EN_CMD12,    /* SD_CMD_AUTO_CMD_EN_CMD12 not in original driver */
    SD_CMD_INDEX(19) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(20) | SD_RESP_R1b,
    SD_CMD_RESERVED(21),
    SD_CMD_RESERVED(22),
    SD_CMD_INDEX(23) | SD_RESP_R1,
    SD_CMD_INDEX(24) | SD_RESP_R1 | SD_DATA_WRITE,
    SD_CMD_INDEX(25) | SD_RESP_R1 | SD_DATA_WRITE | SD_CMD_MULTI_BLOCK | SD_CMD_BLKCNT_EN | SD_CMD_AUTO_CMD_EN_CMD12,   /* SD_CMD_AUTO_CMD_EN_CMD12 not in original driver */
    SD_CMD_RESERVED(26),
    SD_CMD_INDEX(27) | SD_RESP_R1 | SD_DATA_WRITE,
    SD_CMD_INDEX(28) | SD_RESP_R1b,
    SD_CMD_INDEX(29) | SD_RESP_R1b,
    SD_CMD_INDEX(30) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_RESERVED(31),
    SD_CMD_INDEX(32) | SD_RESP_R1,
    SD_CMD_INDEX(33) | SD_RESP_R1,
    SD_CMD_RESERVED(34),
    SD_CMD_RESERVED(35),
    SD_CMD_RESERVED(36),
    SD_CMD_RESERVED(37),
    SD_CMD_INDEX(38) | SD_RESP_R1b,
    SD_CMD_RESERVED(39),
    SD_CMD_RESERVED(40),
    SD_CMD_RESERVED(41),
    SD_CMD_RESERVED(42) | SD_RESP_R1,
    SD_CMD_RESERVED(43),
    SD_CMD_RESERVED(44),
    SD_CMD_RESERVED(45),
    SD_CMD_RESERVED(46),
    SD_CMD_RESERVED(47),
    SD_CMD_RESERVED(48),
    SD_CMD_RESERVED(49),
    SD_CMD_RESERVED(50),
    SD_CMD_RESERVED(51),
    SD_CMD_RESERVED(52),
    SD_CMD_RESERVED(53),
    SD_CMD_RESERVED(54),
    SD_CMD_INDEX(55) | SD_RESP_R1,
    SD_CMD_INDEX(56) | SD_RESP_R1 | SD_CMD_ISDATA,
    SD_CMD_RESERVED(57),
    SD_CMD_RESERVED(58),
    SD_CMD_RESERVED(59),
    SD_CMD_RESERVED(60),
    SD_CMD_RESERVED(61),
    SD_CMD_RESERVED(62),
    SD_CMD_RESERVED(63)
};

const uint32_t sd_acommands[] = {
    SD_CMD_RESERVED(0),
    SD_CMD_RESERVED(1),
    SD_CMD_RESERVED(2),
    SD_CMD_RESERVED(3),
    SD_CMD_RESERVED(4),
    SD_CMD_RESERVED(5),
    SD_CMD_INDEX(6) | SD_RESP_R1,
    SD_CMD_RESERVED(7),
    SD_CMD_RESERVED(8),
    SD_CMD_RESERVED(9),
    SD_CMD_RESERVED(10),
    SD_CMD_RESERVED(11),
    SD_CMD_RESERVED(12),
    SD_CMD_INDEX(13) | SD_RESP_R1,
    SD_CMD_RESERVED(14),
    SD_CMD_RESERVED(15),
    SD_CMD_RESERVED(16),
    SD_CMD_RESERVED(17),
    SD_CMD_RESERVED(18),
    SD_CMD_RESERVED(19),
    SD_CMD_RESERVED(20),
    SD_CMD_RESERVED(21),
    SD_CMD_INDEX(22) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(23) | SD_RESP_R1,
    SD_CMD_RESERVED(24),
    SD_CMD_RESERVED(25),
    SD_CMD_RESERVED(26),
    SD_CMD_RESERVED(27),
    SD_CMD_RESERVED(28),
    SD_CMD_RESERVED(29),
    SD_CMD_RESERVED(30),
    SD_CMD_RESERVED(31),
    SD_CMD_RESERVED(32),
    SD_CMD_RESERVED(33),
    SD_CMD_RESERVED(34),
    SD_CMD_RESERVED(35),
    SD_CMD_RESERVED(36),
    SD_CMD_RESERVED(37),
    SD_CMD_RESERVED(38),
    SD_CMD_RESERVED(39),
    SD_CMD_RESERVED(40),
    SD_CMD_INDEX(41) | SD_RESP_R3,
    SD_CMD_INDEX(42) | SD_RESP_R1,
    SD_CMD_RESERVED(43),
    SD_CMD_RESERVED(44),
    SD_CMD_RESERVED(45),
    SD_CMD_RESERVED(46),
    SD_CMD_RESERVED(47),
    SD_CMD_RESERVED(48),
    SD_CMD_RESERVED(49),
    SD_CMD_RESERVED(50),
    SD_CMD_INDEX(51) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_RESERVED(52),
    SD_CMD_RESERVED(53),
    SD_CMD_RESERVED(54),
    SD_CMD_RESERVED(55),
    SD_CMD_RESERVED(56),
    SD_CMD_RESERVED(57),
    SD_CMD_RESERVED(58),
    SD_CMD_RESERVED(59),
    SD_CMD_RESERVED(60),
    SD_CMD_RESERVED(61),
    SD_CMD_RESERVED(62),
    SD_CMD_RESERVED(63)
};

static int emmc_ensure_data_mode(struct emmc *self);
static int emmc_do_data_command(struct emmc *self, int is_write,
                                uint8_t * buf, size_t buf_size,
                                uint32_t block_no);
static size_t emmc_do_read(struct emmc *self, uint8_t * buf,
                           size_t buf_size, uint32_t block_no);
static size_t emmc_do_write(struct emmc *self, uint8_t * buf,
                            size_t buf_size, uint32_t block_no);

static int emmc_card_init(struct emmc *self);
static int emmc_card_reset(struct emmc *self);

static int emmc_issue_command(struct emmc *self, uint32_t cmd,
                              uint32_t arg, int timeout);
static void emmc_issue_command_int(struct emmc *self, uint32_t reg,
                                   uint32_t arg, int timeout);

static int emmc_set_host_data(struct emmc *self);
static void emmc_power_noreg(struct emmc *self, uint8_t mode, uint16_t vdd);
static void emmc_voltage_restore(struct emmc *self, bool sd_off);
static void emmc_setup_pad(struct emmc *self, bool sd_off);
static void emmc_setup_io(struct emmc *self, bool reset);
static void emmc_power_on_off(struct emmc *self, uint8_t mode, uint16_t vdd);

static int emmc_timeout_wait(uint64_t reg, uint32_t mask, int value, uint64_t us);
static int emmc_switch_clock_rate(struct emmc *self, uint32_t clock);
static int emmc_reset_cmd(void);
static int emmc_reset_dat(void);
static void emmc_handle_card_interrupt(struct emmc *self);
static void emmc_handle_interrupts(struct emmc *self);

static inline uint32_t be2le32(uint32_t x)
{
    return ((x & 0x000000FF) << 24)
        | ((x & 0x0000FF00) << 8)
        | ((x & 0x00FF0000) >> 8)
        | ((x & 0xFF000000) >> 24);
}

static void emmc_dump_host(struct sdhci_host *host)
{
    debug("struct sdhci_host");
    debug(" caps    : %032b", host->host_caps);
    debug(" version : %d", host->version);
    debug(" max_clk : %d", host->max_clk);
    debug(" f_min   : %d", host->f_min);
    debug(" f_max   : %d", host->f_max);
    debug(" b_max   : %d", host->b_max);
    debug(" voltages: %032b", host->voltages);
    debug(" vol_18v : %s", host->vol_18v ? "true" : "false");
}

static void emmc_dump_mmc(struct emmc *self)
{
    debug("struct mmc");
    debug(" ull_offset            : %lld", self->ull_offset);
    debug(" hci_ver               : %d", self->hci_ver);
    debug(" device_id             : [%d, %d, %d, %d]", self->device_id[0], self->device_id[1], self->device_id[2], self->device_id[3]);
    debug(" card_supports_sdhc    : %d", self->card_supports_sdhc);
    debug(" card_supports_hs      : %d", self->card_supports_hs);
    debug(" card_supports_18v     : %d", self->card_supports_18v);
    debug(" card_ocr              : 0x%08x", self->card_ocr);
    debug(" card_rca              : 0x%08x", self->card_rca);
    debug(" last_interrupt        : 0x%08x", self->last_interrupt);
    debug(" last_error            :%d", self->last_error);
    debug(" failed_voltage_switch : %d", self->failed_voltage_switch);
    debug(" last_cmd_reg          : 0x%08x", self->last_cmd_reg);
    debug(" last_cmd              : 0x%08x", self->last_cmd);
    debug(" last_cmd_success      : %d", self->last_cmd_success);
    debug(" last_r0               : 0x%08x", self->last_r0);
    debug(" last_r1               : 0x%08x", self->last_r1);
    debug(" last_r2               : 0x%08x", self->last_r2);
    debug(" last_r3               : 0x%08x", self->last_r3);
    debug(" blocks_to_transfer    : %d", self->blocks_to_transfer);
    debug(" block_size            : %d", self->block_size);
    debug(" card_removal          : %d", self->card_removal);
    debug(" pwr                   : %d", self->pwr);

    emmc_dump_host(&self->host);
}


void emmc_intr(struct emmc *self)
{
}

void emmc_clear_interrupt(void)
{
    uint32_t irpts = read32(SD_INT_STS);
    debug("irpts: 0x%x", irpts);
    write32(SD_INT_STS, irpts);
}

//int emmc_init(struct emmc *self, void (*sleep_fn)(void *), void *sleep_arg)
int emmc_init(struct emmc *self)
{
    if (emmc_card_init(self) != 0) {
        debug("faile emmc_card_init");
        return -1;
    }
    return 0;
}

size_t emmc_read(struct emmc *self, void *buf, size_t cnt)
{
    if (self->ull_offset % SD_BLOCK_SIZE != 0) {
        return -1;
    }
    uint32_t nblock = self->ull_offset / SD_BLOCK_SIZE;
    debug("nblock: 0x%x, ull_offset: 0x%llx", nblock, self->ull_offset);
    if (emmc_do_read(self, (uint8_t *) buf, cnt, nblock) != cnt) {
        return -1;
    }
    return cnt;
}

size_t emmc_write(struct emmc *self, void *buf, size_t cnt)
{
    if (self->ull_offset % SD_BLOCK_SIZE != 0) {
        return -1;
    }
    uint32_t nblock = self->ull_offset / SD_BLOCK_SIZE;

    if (emmc_do_write(self, (uint8_t *) buf, cnt, nblock) != cnt) {
        return -1;
    }
    return cnt;
}

uint64_t emmc_seek(struct emmc *self, uint64_t off)
{
    self->ull_offset = off;
    return off;
}

/* バスパワーのオン/オフ */
static void emmc_power_noreg(struct emmc *self, uint8_t mode, uint16_t vdd)
{
    uint8_t pwr = 0;

    if (mode != POWER_OFF_MODE) {
        switch (1 << vdd) {
        case SD_VDD_165_195:
        case SD_VDD_20_21:
            pwr = SD_POWER_180;
            break;
        case SD_VDD_29_30:
        case SD_VDD_30_31:
            pwr = SD_POWER_300;
            break;
        case SD_VDD_32_33:
        case SD_VDD_33_34:
            pwr = SD_POWER_330;
            break;
        default:
            warn("invalid vdd 0x%x", vdd);
            break;
        }
    }
    debug("self->pwr: 0x%02x, pwr: 0x%02x", self->pwr, pwr);

    /* すでにセットされている場合は何もしない */
    if (self->pwr == pwr)
        return;

    self->pwr = pwr;

    if (pwr == 0) {
        /* bus powerオフ */
        write8(SD_BUS_POWER_CNTL, 0);
    } else {
        /* 電圧設定と電源オン */
        write8(SD_BUS_POWER_CNTL, pwr | SD_POWER_ON);
        delayms(1);
        debug("SD_BUS_POWER_CNTL: 0x%02x", read8(SD_BUS_POWER_CNTL));
    }
}

static void emmc_voltage_restore(struct emmc *self, bool sd_off)
{
    if (sd_off) {
        /* Voltage close flow: 1.8Vを止める（pwrswオフ） */
        //(reg_pwrsw_auto=1, reg_pwrsw_disc=1, reg_pwrsw_vsel=1(1.8v), reg_en_pwrsw=0)
        write32(SYSCTL_SD_PWRSW_CTRL, 0xE | (read32(SYSCTL_SD_PWRSW_CTRL) & 0xFFFFFFF0));
        self->host.vol_18v = false;
    } else {
        if (!self->host.vol_18v) {
            //Voltage switching flow (3.3)
            //(reg_pwrsw_auto=1, reg_pwrsw_disc=0, reg_pwrsw_vsel=0(3.3v), reg_en_pwrsw=1)
            write32(SYSCTL_SD_PWRSW_CTRL, 0x9 | (read32(SYSCTL_SD_PWRSW_CTRL) & 0xFFFFFFF0));
            delayms(1);
            debug("SYSCTL_SD_PWRSW_CTRL: 0x%08x", read32(SYSCTL_SD_PWRSW_CTRL));
        }
    }

    delayms(1);

    /* DS/HS設定を復元する */
    write32(CVI_SDHCI_SD_CTRL, read32(CVI_SDHCI_SD_CTRL) | LATANCY_1T | SD_RSTN | SD_RStN_OEN);
    write32(CVI_SDHCI_PHY_TX_RX_DLY, 0x1000100);
    write32(CVI_SDHCI_PHY_CONFIG, REG_TX_BPS_SEL_BYPASS);

    delayms(1);
    debug("CVI_SDHCI_SD_CTRL      : 0x%08x", read32(CVI_SDHCI_SD_CTRL));
    debug("CVI_SDHCI_PHY_TX_RX_DLY: 0x%08x,", read32(CVI_SDHCI_PHY_TX_RX_DLY));
    debug("CVI_SDHCI_PHY_CONFIG   : 0x%08x", read32(CVI_SDHCI_PHY_CONFIG));
}

/* SD PADの設定: power on時にbunplug=false, off時にtrue */
static void emmc_setup_pad(struct emmc *self, bool sd_off)
{
    uint8_t val = (sd_off) ? PAD_MUX_GPIO : PAD_MUX_SD0;
    write8(PAD_SD0_PWR_EN, PAD_MUX_SD0);
    write8(PAD_SD0_CLK, val);
    write8(PAD_SD0_CMD, val);
    write8(PAD_SD0_D0, val);
    write8(PAD_SD0_D1, val);
    write8(PAD_SD0_D2, val);
    write8(PAD_SD0_D3, val);
    write8(PAD_SD0_CD, val);
    delayms(1);
    debug("PAD_SD0");
    debug(" PWR_EN: %d, CLK:  %d, CMD:  %d, D0:  %d, D1:  %d, D2:  %d, D3:  %d, CD:  %d", read8(PAD_SD0_PWR_EN), read8(PAD_SD0_CLK), read8(PAD_SD0_CMD), read8(PAD_SD0_D0), read8(PAD_SD0_D1), read8(PAD_SD0_D2), read8(PAD_SD0_D3), read8(PAD_SD0_CD));
}

/* power on時にreset=false, off時にtrue */
static void emmc_setup_io(struct emmc *self, bool reset)
{
    /*                     reset    bit[3:2]
     * Name               T     F
     * REG_SDIO0_CD      PU    PU   01  01
     * REG_SDIO0_PWR_EN  PD    PD   10  10
     * REG_SDIO0_CLK     PD    PD   10  10
     * REG_SDIO0_CMD     PD    PU   10  01
     * REG_SDIO0_D0      PD    PU   10  01
     * REG_SDIO0_D1      PD    PU   10  01
     * REG_SDIO0_D2      PD    PU   10  01
     * REG_SDIO0_D3      PD    PU   10  01
     * IOBLKC_PU_BIT : PU   enable(1)/disable(0)
     * IOBLKC_PD_BIT : PD   enable(1)/disable(0)
     */

    uint8_t pu = 0x04;
    uint8_t pd = 0x80;
    uint8_t mask = 0x0c0;

    write8(IOBLK_SD0_CD,     (read8(IOBLK_SD0_CD)     & ~mask) | pu);
    write8(IOBLK_SD0_PWR_EN, (read8(IOBLK_SD0_PWR_EN) & ~mask) | pd);
    write8(IOBLK_SD0_CLK,    (read8(IOBLK_SD0_CLK)    & ~mask) | pd);
    write8(IOBLK_SD0_CMD,    (read8(IOBLK_SD0_CMD)    & ~mask) | (reset ? pd : pu));
    write8(IOBLK_SD0_DAT0,   (read8(IOBLK_SD0_DAT0)   & ~mask) | (reset ? pd : pu));
    write8(IOBLK_SD0_DAT1,   (read8(IOBLK_SD0_DAT1)   & ~mask) | (reset ? pd : pu));
    write8(IOBLK_SD0_DAT2,   (read8(IOBLK_SD0_DAT2)   & ~mask) | (reset ? pd : pu));
    write8(IOBLK_SD0_DAT3,   (read8(IOBLK_SD0_DAT3)   & ~mask) | (reset ? pd : pu));
    delayms(1);
    debug("IOBLK_SD0");
    debug(" PWR_EN: %d, CLK: %d, CMD: %d, D0: %d, D1: %d, D2: %d, D3: %d, CD: %d", read8(IOBLK_SD0_PWR_EN), read8(IOBLK_SD0_CLK), read8(IOBLK_SD0_CMD), read8(IOBLK_SD0_DAT0), read8(IOBLK_SD0_DAT1), read8(IOBLK_SD0_DAT2), read8(IOBLK_SD0_DAT3), read8(IOBLK_SD0_CD));
}

static void emmc_power_on_off(struct emmc *self, uint8_t mode, uint16_t vdd)
{
    debug("mode: %d, vdd: 0x%04x", mode, vdd);

    /* モードが電源ON */
    if (mode == POWER_ON_MODE) {
        /* 1. バスパワーをオン */
        emmc_power_noreg(self, mode, vdd);
        /* 2. 電圧を3.3Vにする */
        emmc_voltage_restore(self, false);
        /* 3. sd padをsd0にする */
        emmc_setup_pad(self, false);
        /* 4. pu/pdの設定 */
        emmc_setup_io(self, false);
        delayms(5);
    } else if (mode == POWER_OFF_MODE) {
        emmc_setup_pad(self, true);
        emmc_setup_io(self, true);
        emmc_voltage_restore(self, true);
        emmc_power_noreg(self, mode, vdd);
        delayms(30);
    }
}

/** regのmaskビットがvalue値になるのをus待つ.
 *  指定時間内にvalue値になったら0, ならなあったら -1 を返す
 */
static int emmc_timeout_wait(uint64_t reg, uint32_t mask, int value, uint64_t us)
{
    uint64_t start = r_time();
    uint64_t dt = us * US_INTERVAL;
    do {
        if ((read32(reg) & mask) ? value : !value) {
            return 0;
        }
    } while ((r_time() - start) < dt);

    return -1;
}

/* Switch the clock rate whilst running */
static int emmc_switch_clock_rate(struct emmc *self, uint32_t clock)
{
    struct sdhci_host *host = &(self->host);
    unsigned int div, clk = 0;

    debug("Set clock %d, self->max_clk %d", clock, host->max_clk);

    /* Wait max 20 ms */
    if (emmc_timeout_wait(SD_PRESENT_STS, CMD_INHIBIT | DAT_INHIBIT, 0, 200000) < 0) {
        error("Timeout to wait cmd & data inhibit");
        return -EBUSY;
    }

    write16(SD_CONTROL1, 0);
    if (clock == 0)
        return 0;

    if ((host->version & SPEC_VER_MASK) >= SPEC_300) {
        if (host->clk_mul) {
            for (div = 1; div <= 1024; div++) {
                if ((host->max_clk / div) <= clock)
                    break;
            }

            /* CV180XはProgrammable clock mode bit (5)はresearved */
            /* clk = (1 << 5); */
            div--;
        } else {
            /* Version 3.00の除数の倍数でなければならない */
            if (host->max_clk <= clock) {
                div = 1;
            } else {
                for (div = 2;
                     div < SD_MAX_DIV_SPEC_300;
                     div += 2) {
                    if ((host->max_clk / div) <= clock)
                        break;
                }
            }
            div >>= 1;
        }
    } else {
        /* Version 2.00の除数の倍数でなければならない */
        for (div = 1; div < SD_MAX_DIV_SPEC_200; div *= 2) {
            if ((host->max_clk / div) <= clock)
                break;
        }
        div >>= 1;
    }

    debug("clk div 0x%x\n", div);

    clk |= (div & SD_DIV_MASK) << SD_DIVIDER_SHIFT;
    clk |= ((div & SD_DIV_HI_MASK) >> SD_DIV_MASK_LEN)
        << SD_DIVIDER_HI_SHIFT;
    clk |= SD_CLOCK_INT_EN;

    debug("SD_CONTROL1: 0x%x\n", clk);
    write16(SD_CONTROL1, clk);

    /* Wait max 20 ms */
    if (emmc_timeout_wait(SD_CONTROL1, SD_CLOCK_INT_STABLE, 1, 200000) < 0) {
        error("Internal clock never stabilised.");
        return -EBUSY;
    }

    clk |= SD_CLOCK_CARD_EN;
    write16(SD_CONTROL1, clk);
    return 0;
}

/* CMDラインをリセット */
static int emmc_reset_cmd(void)
{
    write32(SD_CONTROL1, read32(SD_CONTROL1) | SD_RESET_CMD);

    if (emmc_timeout_wait(SD_CONTROL1, SD_RESET_CMD, 0, 1000000) < 0) {
        error("CMD line did not reset properly");
        return -1;
    }
    return 0;
}

/* データラインをリセット */
static int emmc_reset_dat(void)
{
    write32(SD_CONTROL1, read32(SD_CONTROL1) | SD_RESET_DAT);

    if (emmc_timeout_wait(SD_CONTROL1, SD_RESET_DAT, 0, 1000000) < 0) {
        error("DAT line did not reset properly");
        return -1;
    }
    return 0;
}

static void emmc_issue_command_int(struct emmc *self, uint32_t cmd_reg,
                       uint32_t argument, int timeout)
{
    self->last_cmd_reg = cmd_reg;
    self->last_cmd_success = 0;

    /* This is as per HCSS 3.7.1.1/3.7.2.2 */
    /* nodef */
#ifdef SD_POLL_STATUS_REG
    /* Check Command Inhibit */
    while (read32(SD_PRESENT_STS) & 1) {
        delayus(1000);
    }

    /* Is the command with busy? */
    if ((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) {
        /* With busy */

        /* Is is an abort command? */
        if ((cmd_reg & SD_CMD_TYPE_MASK) != SD_CMD_TYPE_ABORT) {
            /* Not an abort command */

            /* Wait for the data line to be free */
            while (read32(SD_PRESENT_STS) & 2) {
                delayus(1000);
            }
        }
    }
#endif

    /* Set block size and block count */
    /* For now, block size = 512 bytes, block count = 1, */
    if (self->blocks_to_transfer > 0xffff) {
        debug("blocks_to_transfer too great (%d)",
              self->blocks_to_transfer);
        self->last_cmd_success = 0;
        return;
    }
    uint32_t blksizecnt =
        self->block_size | (self->blocks_to_transfer << 16);
    write32(SD_BLKSIZECNT, blksizecnt);

    /* Set argument 1 reg */
    write32(SD_ARG1, argument);

    /* Set command reg */
    write32(SD_CMDTM, cmd_reg);

    /* delayus(2000); */

    /* コマンド完了（エラー発生を含む）を待機 */
    emmc_timeout_wait(SD_INT_STS, 0x8001, 1, 10000000);
    uint32_t irpts = read32(SD_INT_STS);

    /* コマンド完了フラグをクリア */
    write32(SD_INT_STS, 0xffff0001);

    /* Test for errors */
    if ((irpts & 0xffff0001) != 1) {
        error("error waiting for command complete interrupt 1: irpts=0b%032b", irpts);
        self->last_error = irpts & 0xffff0000;
        self->last_interrupt = irpts;
        return;
    }

    /* delayus(2000); */

    /* Get response data */
    switch (cmd_reg & SD_CMD_RSPNS_TYPE_MASK) {
    case SD_CMD_RSPNS_TYPE_48:
    case SD_CMD_RSPNS_TYPE_48B:
        self->last_r0 = read32(SD_RESP0);
        break;

    case SD_CMD_RSPNS_TYPE_136:
        self->last_r0 = read32(SD_RESP0);
        self->last_r1 = read32(SD_RESP1);
        self->last_r2 = read32(SD_RESP2);
        self->last_r3 = read32(SD_RESP3);
        break;
    }

    /* If with data, wait for the appropriate interrupt */
    if (cmd_reg & SD_CMD_ISDATA) {
        uint32_t wr_irpt;
        int is_write = 0;
        if (cmd_reg & SD_CMD_DAT_DIR_CH) {
            wr_irpt = (1 << 5); /* read */
        } else {
            is_write = 1;
            wr_irpt = (1 << 4); /* write */
        }

        if (self->blocks_to_transfer > 1) {
            debug("multi-block transfer");
        }

        //assert(((uint64_t) self->buf & 3) == 0);
        uint32_t *pData = (uint32_t *) self->buf;

        for (int nBlock = 0; nBlock < self->blocks_to_transfer; nBlock++) {
            emmc_timeout_wait(SD_INT_STS, wr_irpt | 0x8000, 1,
                              timeout);
            irpts = read32(SD_INT_STS);
            write32(SD_INT_STS, 0xffff0000 | wr_irpt);

            if ((irpts & (0xffff0000 | wr_irpt)) != wr_irpt) {
                error("error occured whilst waiting for data ready interrupt 2");

                self->last_error = irpts & 0xffff0000;
                self->last_interrupt = irpts;
                return;
            }

            /* Transfer the block */
            assert(self->block_size <= 1024);   /* internal FIFO size of EMMC */
            size_t length = self->block_size;
            assert((length & 3) == 0);

            if (is_write) {
                for (; length > 0; length -= 4) {
                    write32(SD_DATA, *pData++);
                }
            } else {
                for (; length > 0; length -= 4) {
                    *pData++ = read32(SD_DATA);
                }
            }
        }
        debug("block transfer complete");
    }

    /* Wait for transfer complete (set if read/write transfer or with busy) */
    if ((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B
        || (cmd_reg & SD_CMD_ISDATA)) {
#ifdef SD_POLL_STATUS_REG
        /* First check command inhibit (DAT) is not already 0 */
        if ((read32(SD_PRESENT_STS) & 2) == 0) {
            write32(SD_INT_STS, 0xffff0002);
        } else
#endif
        {
            emmc_timeout_wait(SD_INT_STS, 0x8002, 1, timeout);
            irpts = read32(SD_INT_STS);
            write32(SD_INT_STS, 0xffff0002);

            /* Handle the case where both data timeout and transfer complete */
            /*  are set - transfer complete overrides data timeout: HCSS 2.2.17 */
            if (((irpts & 0xffff0002) != 2)
                && ((irpts & 0xffff0002) != 0x100002)) {
                error("error occured whilst waiting for transfer complete interrupt 3");
                self->last_error = irpts & 0xffff0000;
                self->last_interrupt = irpts;
                return;
            }

            write32(SD_INT_STS, 0xffff0002);
        }
    }

    /* Return success */
    debug("emmc_issue_command_int: %d is success!", SD_CMD_INDEX(cmd_reg));
    self->last_cmd_success = 1;
}

/* Handle a card interrupt. */
static void emmc_handle_card_interrupt(struct emmc *self)
{
    uint32_t status = read32(SD_PRESENT_STS);
    debug("status: 0x%x", status);

    /* Get the card status */
    if (self->card_rca) {
        emmc_issue_command_int(self, sd_commands[SEND_STATUS],
                               self->card_rca << 16, 500000);
        if (FAIL(self)) {
            warn("unable to get card status");
        } else {
            debug("card status: 0x%x", self->last_r0);
        }
    } else {
        debug("no card currently selected");
    }
}

static void emmc_handle_interrupts(struct emmc *self)
{
    uint32_t irpts = read32(SD_INT_STS);
    /* 未処理の割り込みがなければ何もしない */
    if (irpts == 0) return;

    debug("SD_INT_STS: 0x%08x", irpts);
    uint32_t reset_mask = 0;

    if (irpts & SD_COMMAND_COMPLETE) {
        trace("spurious command complete interrupt");
        reset_mask |= SD_COMMAND_COMPLETE;
    }

    if (irpts & SD_TRANSFER_COMPLETE) {
        trace("spurious transfer complete interrupt");
        reset_mask |= SD_TRANSFER_COMPLETE;
    }

    if (irpts & SD_BLOCK_GAP_EVENT) {
        trace("spurious block gap event interrupt");
        reset_mask |= SD_BLOCK_GAP_EVENT;
    }

    if (irpts & SD_DMA_INTERRUPT) {
        trace("spurious DMA interrupt");
        reset_mask |= SD_DMA_INTERRUPT;
    }

    if (irpts & SD_BUFFER_WRITE_READY) {
        trace("spurious buffer write ready interrupt");
        reset_mask |= SD_BUFFER_WRITE_READY;
        emmc_reset_dat();
    }

    if (irpts & SD_BUFFER_READ_READY) {
        trace("spurious buffer read ready interrupt");
        reset_mask |= SD_BUFFER_READ_READY;
        emmc_reset_dat();
    }

    if (irpts & SD_CARD_INSERTION) {
        trace("card insertion detected");
        reset_mask |= SD_CARD_INSERTION;
    }

    if (irpts & SD_CARD_REMOVAL) {
        trace("card removal detected");
        reset_mask |= SD_CARD_REMOVAL;
        self->card_removal = 1;
    }

    if (irpts & SD_CARD_INTERRUPT) {
        debug("card interrupt detected");
        emmc_handle_card_interrupt(self);
        reset_mask |= SD_CARD_INTERRUPT;
    }

    if (irpts & 0x8000) {
        //debug("spurious error interrupt: 0x%x", irpts);
        reset_mask |= 0xffff0000;
    }

    write32(SD_INT_STS, reset_mask);
}

static void emmc_dump_clk_ctl(uint32_t ctl)
{
    debug("CLK_CTL_SWRST");
    debug(" int_clk_en    : %d", (ctl >>  0) & 0x01);
    debug(" int_clk_stable: %d", (ctl >>  1) & 0x01);
    debug(" sd_clk_em     : %d", (ctl >>  2) & 0x01);
    debug(" pll_en        : %d", (ctl >>  3) & 0x01);
    debug(" up_freq_sel   : %d", (ctl >>  6) & 0x03);
    debug(" freq_sel      : 0x%02x", (ctl >>  8) & 0xff);
    debug(" tout_cnt      : 0x%x", (ctl >> 16) & 0x0f);
    debug(" sw_rst_all    : %d", (ctl >> 24) & 0x01);
    debug(" sw_rst_cmd    : %d", (ctl >> 25) & 0x01);
    debug(" sw_rst_dat    : %d", (ctl >> 26) & 0x01);
}

static void emmc_dump_host_ctl1(uint32_t ctl1)
{
    debug("HOST_CTL1");
    debug(" lec_ctl              : %d", (ctl1 >>  0) & 0x01);
    debug(" dat_xfer_width       : %d", (ctl1 >>  1) & 0x01);
    debug(" hs_enable            : %d", (ctl1 >>  2) & 0x01);
    debug(" dma_sel              : %d", (ctl1 >>  3) & 0x03);
    debug(" ext_dat_width        : %d", (ctl1 >>  5) & 0x01);
    debug(" card_det_test        : %d", (ctl1 >>  6) & 0x01);
    debug(" card_det_sel         : %d", (ctl1 >>  7) & 0x01);
    debug(" sd_bus_pwr           : %d", (ctl1 >>  8) & 0x01);
    debug(" sd_bus_vol_sel       : b%03b", (ctl1 >> 9) & 0x07);
    debug(" stop_bg_req          : %d", (ctl1 >> 16) & 0x01);
    debug(" continue_req         : %d", (ctl1 >> 17) & 0x01);
    debug(" read_wait            : %d", (ctl1 >> 18) & 0x01);
    debug(" int_bg               : %d", (ctl1 >> 19) & 0x01);
    debug(" wakeup_on_card_int   : %d", (ctl1 >> 24) & 0x01);
    debug(" wakeup_on_card_insert: %d", (ctl1 >> 25) & 0x01);
    debug(" wakeup_on_card_ready : %d", (ctl1 >> 26) & 0x01);
}

static void emmc_host_ctl2(uint32_t value)
{
    uint16_t ctl2 = (uint16_t)(value >> 16);

    debug("HOST_CTL2");
    debug(" uhs_mode_sel     : %d", (ctl2 >>  0) & 0x07);
    debug(" en_18_sig        : %d", (ctl2 >>  3) & 0x01);
    debug(" drv_sel          : %d", (ctl2 >>  4) & 0x03);
    debug(" execute_tune     : %d", (ctl2 >>  6) & 0x01);
    debug(" sample_clk_sel   : %d", (ctl2 >>  7) & 0x01);
    debug(" async_int_en     : %d", (ctl2 >> 14) & 0x01);
    debug(" preset_val_enable: %d", (ctl2 >> 15) & 0x01);
}

static void mmc_dump_caps1(uint32_t caps1)
{
    debug("SD_CAPABILITIES_1");
    debug(" tout_clk_freq    : %d", (caps1 >>  0) & 0x3f);
    debug(" tout_clk_unit    : %d", (caps1 >>  7) & 0x01);
    debug(" base_clk_freq    : %d", (caps1 >>  8) & 0xff);
    debug(" max_blk_len      : %d", (caps1 >> 16) & 0x03);
    debug(" embeeded_8bit    : %d", (caps1 >> 18) & 0x01);
    debug(" adaa2_support    : %d", (caps1 >> 19) & 0x01);
    debug(" hs_support       : %d", (caps1 >> 21) & 0x01);
    debug(" sdma_support     : %d", (caps1 >> 22) & 0x01);
    debug(" sus_res_support  : %d", (caps1 >> 23) & 0x01);
    debug(" v33_support      : %d", (caps1 >> 24) & 0x01);
    debug(" v30_support      : %d", (caps1 >> 25) & 0x01);
    debug(" v18_support      : %d", (caps1 >> 26) & 0x01);
    debug(" bus64_support    : %d", (caps1 >> 28) & 0x01);
    debug(" async_int_support: %d", (caps1 >> 29) & 0x01);
    debug(" slot_type        : %d", (caps1 >> 30) & 0x03);
}

static void mmc_dump_caps2(uint32_t caps2)
{
    debug("SD_CAPABILITIES_2");
    debug(" sdr50_support : %d", (caps2 >> 0) & 0x01);
    debug(" sdr104_support: %d", (caps2 >> 1) & 0x01);
    debug(" ddr50_support : %d", (caps2 >> 2) & 0x01);
    debug(" drv_a_support : %d", (caps2 >> 4) & 0x01);
    debug(" drv_a_support : %d", (caps2 >> 5) & 0x01);
    debug(" drv_a_support : %d", (caps2 >> 6) & 0x01);
    debug(" retune_timer  : 0x%x", (caps2 >> 8) & 0x0f);
    debug(" tune_sdr50    : %d", (caps2 >> 13) & 0x01);
    debug(" retune_mode   : %d", (caps2 >> 14) & 0x03);
    debug(" clk_muliplier : 0x%02x", (caps2 >> 24) & 0xff);
}

static void mmc_dump_version(uint１６_t version)
{
    debug("SD_HOST_VERSION");
    debug(" spec  : %d", (version >> ０) & 0xff);
    debug(" vendor: 0x%02x", (version >> 16) & 0xff);
}


static int emmc_set_host_data(struct emmc *self)
{
    uint32_t caps1, caps2 = 0;
    uint16_t version, spec_ver;
    uint32_t f_min  = 400000;
    uint32_t f_max  = 200000000;

    /* 1. cv180xのSD hostとしての定数値 */
    struct sdhci_host *host = &(self->host);
    host->max_clk = 375000000;
    host->vol_18v = false;

    /* 2. カードのケーパビリティ, バージョン情報を取得 */
    caps1 = read32(SD_CAPABILITIES_1);
    caps2 = read32(SD_CAPABILITIES_2);
    version = read16(SD_HOST_VERSION);

    mmc_dump_caps1(caps1);
    mmc_dump_caps2(caps2);
    mmc_dump_version(version);

    host->version = (uint32_t)version;
    spec_ver = version & SPEC_VER_MASK;

    /* 3. clock multiplierのサポートチェック */
    if (spec_ver >= SPEC_300) {
        host->clk_mul = (caps2 & CAP2_CLK_MUL_MASK) >> CAP2_CLK_MUL_SHIFT;
    }

    if (host->max_clk == 0) {
        if (spec_ver >= SPEC_300)
            host->max_clk = (caps1 & CAP1_CLOCK_V3_BASE_MASK) >> CAP1_CLOCK_BASE_SHIFT;
        else
            host->max_clk = (caps1 & CAP1_CLOCK_BASE_MASK) >> CAP1_CLOCK_BASE_SHIFT;
        host->max_clk *= 1000000;
        if (host->clk_mul)
            host->max_clk *= host->clk_mul;
    }

    if (host->max_clk == 0) {
        error("Hardware doesn't specify base clock frequency");
        return -EINVAL;
    }

    if (f_max && (f_max < host->max_clk))
        host->f_max = f_max;
    else
        host->f_max = host->max_clk;

    if (f_min) {
        host->f_min = f_min;
    } else {
        if (spec_ver >= SPEC_300)
            host->f_min = host->f_max / CAP1_MAX_DIV_SPEC_300;
        else
            host->f_min = host->f_max / CAP1_MAX_DIV_SPEC_200;
    }

    host->voltages = 0;
    if (caps1 & CAP1_CAN_VDD_330)
        host->voltages |= SD_VDD_32_33 | SD_VDD_33_34;
    if (caps1 & CAP1_CAN_VDD_300)
        host->voltages |= SD_VDD_29_30 | SD_VDD_30_31;
    if (caps1 & CAP1_CAN_VDD_180)
        host->voltages |= SD_VDD_165_195;

    if (caps1 & CAP1_CAN_DO_HISPD)
        host->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz;

    host->host_caps |= CAP_MODE_4BIT;

    /* Host Controller Version3.0 以降 */
    if (spec_ver >= SPEC_300) {
        if (!(caps1 & CAP1_CAN_DO_8BIT))
            host->host_caps &= ~CAP_MODE_8BIT;
    }

    if (!(host->voltages & SD_VDD_165_195) || !host->vol_18v)
        caps2 &= ~(CAP2_SUPPORT_SDR104 | CAP2_SUPPORT_SDR50 | CAP2_SUPPORT_DDR50);

    if (!host->vol_18v)
        caps2 &= ~(CAP2_SUPPORT_SDR104 | CAP2_SUPPORT_SDR50 | CAP2_SUPPORT_DDR50);

    if (caps2 & (CAP2_SUPPORT_SDR104 | CAP2_SUPPORT_SDR50 | CAP2_SUPPORT_DDR50))
        host->host_caps |= MMC_CAP(UHS_SDR12) | MMC_CAP(UHS_SDR25);

    if (caps2 & CAP2_SUPPORT_SDR104) {
        host->host_caps |= MMC_CAP(UHS_SDR104) | MMC_CAP(UHS_SDR50);
        /*
         * SD3.0: SDR104 is supported so (for eMMC) the caps2
         * field can be promoted to support HS200.
         */
        host->host_caps |= MMC_CAP(MMC_HS_200);
    } else if (caps2 & CAP2_SUPPORT_SDR50) {
        host->host_caps |= MMC_CAP(UHS_SDR50);
    }

    if (caps2 & CAP2_SUPPORT_DDR50)
        host->host_caps |= MMC_CAP(UHS_DDR50);

    host->b_max = 65535; /* CONFIG_SYS_SD_MAX_BLK_COUNTが未定義の場合のデフォルト */

    emmc_dump_host(host);

    return 0;
}

/* カードの初期化 */
static int emmc_card_init(struct emmc *self)
{
    /* 1. cv180x sd hostの情報取得 */
    emmc_set_host_data(self);
    /* 2. バスパワーオン */
    emmc_power_on_off(self, POWER_ON_MODE, generic_fls(self->host.voltages) - 1);
    delayms(5);
    /* 3. 割り込みを有効化（SDコントローラが提供している割り込みのみ） */
    write32(SD_INT_STS_EN, SD_INT_DATA_MASK | SD_INT_CMD_MASK);
    /* 4 割り込みは有効にするが割り込み信号は出さない */
    write32(SD_INT_SIG_EN, 0x0);
    /* 5. PLLの設定 */
    if (self->host.max_clk == SD_MAX_CLOCK) {
        write32(DIV_CLK_SD0, SD_MAX_CLOCK_DIV_VALUE);
        write32(CLOCK_BYPASS_SELECT, read32(CLOCK_BYPASS_SELECT) & ~(1 << 6));
        debug("PLL Setting");
        debug(" Clock: %d, DIV_CLK_SD0: 0x%08x, CLOCK_BYPASS_SELECT: 0x%08x", self->host.max_clk, read32(CLOCK_BYPASS_SELECT), read32(CLOCK_BYPASS_SELECT))
    }

    /* 6. 64-bitアドレッシングを設定: bit[28-29]は仕様書に情報なし */
    write32(SD_CONTROL2, read32(SD_CONTROL2) | (1 << 28) | (1 << 29));
    delayms(1);
    emmc_host_ctl2(read32(SD_CONTROL2));

    /* 7 Default value */
    write32(CVI_SDHCI_SD_CTRL, LATANCY_1T);
    write32(CVI_SDHCI_PHY_TX_RX_DLY, 0x01000100);
    write32(CVI_SDHCI_PHY_CONFIG, REG_TX_BPS_SEL_BYPASS);

    delayms(1);
    debug("CVI_SDHCI_SD_CTRL      : 0x%08x", read32(CVI_SDHCI_SD_CTRL));
    debug("CVI_SDHCI_PHY_TX_RX_DLY: 0x%08x", read32(CVI_SDHCI_PHY_TX_RX_DLY));
    debug("CVI_SDHCI_PHY_CONFIG   : 0x%08x", read32(CVI_SDHCI_PHY_CONFIG));

    /* Check the sanity of the sd_commands and sd_acommands structures */
    assert(sizeof(sd_commands) == (64 * sizeof(uint32_t)));
    assert(sizeof(sd_acommands) == (64 * sizeof(uint32_t)));

    self->hci_ver = self->host.version & 0xff;
    debug("hci_ver: %d", self->hci_ver);
    if (self->hci_ver < 2) {
#ifdef SD_ALLOW_OLD_SDHCI
        warn("old SDHCI version detected");
#else
        error("only SDHCI versions >= 3.0 are supported");
        return -1;
#endif
    }

    return 0;
}

/* データ転送モードであることを確認する */
static int emmc_ensure_data_mode(struct emmc *self)
{
    /* 1. rcaを未取得の場合はカードをリセット */
    if (self->card_rca == 0) {
        /* Try again to initialise the card */
        int ret = emmc_card_reset(self);
        if (ret != 0) {
            warn("fail emmc_card_reset 1");
            return ret;
        }
    }

    trace("obtaining status register for card_rca 0x%x: ", self->card_rca);
    /* 2. アドレス指定したカードの状態を取得 */
    if (!emmc_issue_command
        (self, SEND_STATUS, self->card_rca << 16, 500000)) {
        error("error sending CMD13");
        self->card_rca = 0;
        return -1;
    }

    uint32_t status = self->last_r0;
    /* 3. レスポンスから現在の状況ビットを取得 */
    uint32_t cur_state = (status >> 9) & 0xf;
    trace("status 0x%x", cur_state);
    if (cur_state == 3) {
        /* Stand-by状態: このカードを選択 */
        if (!emmc_issue_command
            (self, SELECT_CARD, self->card_rca << 16, 500000)) {
            warn("no response from CMD17");
            self->card_rca = 0;
            return -1;
        }
    } else if (cur_state == 5) {
        /* Data Sending 状態 - 転送をキャンセル */
        if (!emmc_issue_command(self, STOP_TRANSMISSION, 0, 500000)) {
            warn("no response from CMD12");
            self->card_rca = 0;
            return -1;
        }
        /* データラインをリセット */
        emmc_reset_dat();
    } else if (cur_state != 4) {
        /* Transfer状態以外: 最初期化. */
        int ret = emmc_card_reset(self);
        if (ret != 0) {
            debug("fail emmc_card_reset 2");
            return ret;
        }
    }

    /* 正しい状態(Transfer状態）にあるか再度チェック */
    if (cur_state != 4) {
        if (!emmc_issue_command
            (self, SEND_STATUS, self->card_rca << 16, 500000)) {
            warn("no response from CMD13");
            self->card_rca = 0;
            return -1;
        }
        status = self->last_r0;
        cur_state = (status >> 9) & 0xf;
        trace("recheck status 0x%x", cur_state);

        if (cur_state != 4) {
            warn("unable to initialise SD card to data mode (state 0x%x)",
                 cur_state);
            self->card_rca = 0;
            return -1;
        }
    }

    return 0;
}

static int emmc_do_data_command(struct emmc *self, int is_write, uint8_t * buf,
                     size_t buf_size, uint32_t block_no)
{

    /* PLSS table 4.20 - SDSC cards use byte addresses rather than block addresses */
    if (!self->card_supports_sdhc) {
        block_no *= SD_BLOCK_SIZE;
    }

    /* This is as per HCSS 3.7.2.1 */
    if (buf_size < self->block_size) {
      /*
        warn("buffer size (%d) less than block size (%d)", buf_size,
             self->block_size);
      */
        return -1;
    }

    self->blocks_to_transfer = buf_size / self->block_size;
    if (buf_size % self->block_size) {
      /*
        warn("buffer size (%d) not a multiple of block size (%d)",
             buf_size, self->block_size);
      */
        return -1;
    }
    self->buf = buf;

    /* Decide on the command to use */
    int command;
    if (is_write) {
        if (self->blocks_to_transfer > 1) {
            command = WRITE_MULTIPLE_BLOCK;
        } else {
            command = WRITE_BLOCK;
        }
    } else {
        if (self->blocks_to_transfer > 1) {
            trace("multi block");
            command = READ_MULTIPLE_BLOCK;
        } else {
            trace("single block");
            command = READ_SINGLE_BLOCK;
        }
    }

    int retry_count = 0;
    int max_retries = 3;
    while (retry_count < max_retries) {
        if (emmc_issue_command(self, command, block_no, 5000000)) {
            break;
        } else {
            error("error sending CMD%d", command);
            trace("error = 0x%x", self->last_error);

            if (++retry_count < max_retries) {
                trace("Retrying");
            } else {
                trace("Giving up");
            }
        }
    }

    if (retry_count == max_retries) {
        self->card_rca = 0;
        return -1;
    }
    return 0;
}


static size_t emmc_do_read(struct emmc *self, uint8_t * buf, size_t buf_size,
             uint32_t block_no)
{
    /* Check the status of the card */
    if (emmc_ensure_data_mode(self) != 0) {
        warn("fail emmc_ensure_data_mode");
        return -1;
    }

    trace("reading from block %u", block_no);

    if (emmc_do_data_command(self, 0, buf, buf_size, block_no) < 0) {
        warn("fail emmc_do_data_command");
        return -1;
    }

    trace("success");
    return buf_size;
}

static size_t emmc_do_write(struct emmc *self, uint8_t * buf, size_t buf_size,
              uint32_t block_no)
{
    /* Check the status of the card */
    if (emmc_ensure_data_mode(self) != 0) {
        return -1;
    }
    trace("writing to block %u", block_no);

    if (emmc_do_data_command(self, 1, buf, buf_size, block_no) < 0) {
        return -1;
    }

    trace("success");

    return buf_size;
}

/** カードのリセット */
static int emmc_card_reset(struct emmc *self)
{
    int ret;

    trace("resetting controller");
    uint32_t control1 = read32(SD_CONTROL1);
    /* CMD/DATA両ラインをソフトリセット */
    control1 |= (1 << 24);
    /* SD clockとinternal clokuを無効化 */
    control1 &= ~(1 << 2);
    control1 &= ~(1 << 0);
    write32(SD_CONTROL1, control1);
    /* ラインがリセットされるのを待つ */
    if (emmc_timeout_wait(SD_CONTROL1, 7 << 24, 0, 1000000) < 0) {
        error("controller did not reset properly");
        return -1;
    }

    /* SDバスパワーを3.3Vでオン */
    uint32_t control0 = read32(SD_CONTROL0);
    control0 |= 0x0F << 8;
    write32(SD_CONTROL0, control0);
    delayus(2000);

    emmc_dump_host_ctl1(read32(SD_CONTROL0));
    emmc_dump_clk_ctl(read32(SD_CONTROL1));

    /* カードがあるかチェック */
    uint32_t status_reg = read32(SD_PRESENT_STS);
    debug("SD_PRESENT_STS: 0x%08x", status_reg);
    if ((status_reg & (1 << 16)) == 0) {
        warn("no card inserted");
        return -1;
    }

    /* control2をすべてクリア */
    //write32(SD_CONTROL2, 0);

    /* クロック周波数を 400KHzに設定 */
    ret = emmc_switch_clock_rate(self, SD_CLOCK_ID);
    if (ret < 0) return ret;

    /* 割り込みフラグをクリア */
    write32(SD_INT_STS, 0xffffffff);
    /* SDカード割り込み以外の割子mを有効にする */
    uint32_t irpt_mask = 0xffffffff & (~SD_CARD_INTERRUPT);
#ifdef SD_CARD_INTERRUPTS
    irpt_mask |= SD_CARD_INTERRUPT;
#endif
    write32(SD_INT_STS_EN, irpt_mask);

    delayus(2000);

    /* >> Prepare the device structure */
    self->device_id[0] = 0;
    self->device_id[1] = 0;
    self->device_id[2] = 0;
    self->device_id[3] = 0;

    self->card_supports_sdhc = 0;
    self->card_supports_hs = 0;
    self->card_supports_18v = 0;
    self->card_ocr = 0;
    self->card_rca = 0;
    self->last_interrupt = 0;
    self->last_error = 0;
    self->failed_voltage_switch = 0;
    self->last_cmd_reg = 0;
    self->last_cmd = 0;
    self->last_cmd_success = 0;
    self->last_r0 = 0;
    self->last_r1 = 0;
    self->last_r2 = 0;
    self->last_r3 = 0;
    self->buf = 0;
    self->blocks_to_transfer = 0;
    self->block_size = 0;
    self->card_removal = 0;

    /* Send CMD0 to the card (reset to idle state) */
    if (!emmc_issue_command(self, GO_IDLE_STATE, 0, 500000)) {
        warn("no CMD0 response");
        return -1;
    }

    /* Send CMD8 to the card */
    /* Voltage supplied = 0x1 = 2.7-3.6V (standard) */
    /* Check pattern = 10101010b (as per PLSS 4.3.13) = 0xAA */
    /* Note a timeout error on the following command (CMD8) is normal and expected if the SD card version is less than 2.0 */
    emmc_issue_command(self, SEND_IF_COND, 0x1aa, 500000);
    int v2_later = 0;
    if (TIMEOUT(self)) {
        v2_later = 0;
    }
    else if (CMD_TIMEOUT(self)) {
        if (emmc_reset_cmd() == -1) {
            return -1;
        }
        write32(SD_INT_STS, SD_ERR_MASK_CMD_TIMEOUT);
        v2_later = 0;
    }
    else if (FAIL(self)) {
        warn("failed to send CMD8");
        return -1;
    } else {
        if ((self->last_r0 & 0xfff) != 0x1aa) {
            warn("unusable card");
            return -1;
        } else {
            v2_later = 1;
        }
    }

    emmc_dump_mmc(self);

    /* ここでCMD5（HCSS 3.6）のレスポンスをチェックする。 */
    /* このレスポンスはカードがSDIOカードの場合にのみ返される。 */
    /* このコマンドがタイムアウトするのは問題はないことに注意。 */
    /* カードがSDIOでない場合に予想されることだからである。 */
    emmc_issue_command(self, IO_SET_OP_COND, 0, 10000);
    if (!TIMEOUT(self)) {
        if (CMD_TIMEOUT(self)) {
            if (emmc_reset_cmd() == -1) {
                warn("fail emmc_rest_cmd");
                return -1;
            }

            write32(SD_INT_STS, SD_ERR_MASK_CMD_TIMEOUT);
        } else
        {
            warn("found SDIO card, unimplemented");
            return -1;
        }
    }

    /* Call an inquiry ACMD41 (voltage window = 0) to get the OCR */
    if (!emmc_issue_command(self, ACMD(41), 0, 500000)) {
        warn("failed to inquiry ACMD41");
        return -1;
    }

    /* Call initialization ACMD41 */
    int busy = 1;
    while (busy) {
        uint32_t v2_flags = 0;
        if (v2_later) {
            /* Set SDHC support */
            v2_flags |= (1 << 30);

            /* Set 1.8v support */
#ifdef SD_1_8V_SUPPORT
            if (!self->failed_voltage_switch) {
                v2_flags |= (1 << 24);
            }
#endif
#ifdef SDXC_MAXIMUM_PERFORMANCE
            /* Enable SDXC maximum performance */
            v2_flags |= (1 << 28);
#endif
        }

        if (!emmc_issue_command
            (self, ACMD(41), 0x00ff8000 | v2_flags, 500000)) {
            error("error issuing ACMD41");
            return -1;
        }

        if ((self->last_r0 >> 31) & 1) {
            /* Initialization is complete */
            self->card_ocr = (self->last_r0 >> 8) & 0xffff;
            self->card_supports_sdhc = (self->last_r0 >> 30) & 0x1;
#ifdef SD_1_8V_SUPPORT
            if (!self->failed_voltage_switch) {
                self->card_supports_18v = (self->last_r0 >> 24) & 0x1;
            }
#endif
            busy = 0;
        } else {
            /* Card is still busy */
            delayus(500000);
        }

    }

    debug("OCR: 0x%x, 1.8v support: %d, SDHC support: %d", self->card_ocr,
          self->card_supports_18v, self->card_supports_sdhc);

    /* At this point, we know the card is definitely an SD card, so will definitely */
    /* support SDR12 mode which runs at 25 MHz */
    emmc_switch_clock_rate(self, SD_CLOCK_NORMAL);

    /* A small wait before the voltage switch */
    delayus(5000);

    /* Switch to 1.8V mode if possible */
    if (self->card_supports_18v) {
        trace("switching to 1.8V mode");
        /* As per HCSS 3.6.1 */

        /* Send VOLTAGE_SWITCH */
        if (!emmc_issue_command(self, VOLTAGE_SWITCH, 0, 500000)) {
            error("error issuing VOLTAGE_SWITCH");
            self->failed_voltage_switch = 1;
            emmc_power_on_off(self, POWER_OFF_MODE, 0);
            return -1;
        }

        /* Disable SD clock */
        control1 = read32(SD_CONTROL1);
        control1 &= ~(1 << 2);
        write32(SD_CONTROL1, control1);

        /* Check DAT[3:0] */
        status_reg = read32(SD_PRESENT_STS);
        uint32_t dat30 = (status_reg >> 20) & 0xf;
        if (dat30 != 0) {
            error("DAT[3:0] did not settle to 0");
            self->failed_voltage_switch = 1;
            emmc_power_on_off(self, POWER_OFF_MODE, 0);
            return -1;
        }

        /* Set 1.8V signal enable to 1 */
        uint32_t control0 = read32(SD_CONTROL0);
        control0 |= (1 << 8);
        write32(SD_CONTROL0, control0);

        /* Wait 5 ms */
        delayus(5000);

        /* Check the 1.8V signal enable is set */
        control0 = read32(SD_CONTROL0);
        if (((control0 >> 8) & 1) == 0) {
            warn("controller did not keep 1.8V signal enable high");
            self->failed_voltage_switch = 1;
            emmc_power_on_off(self, POWER_OFF_MODE, 0);
            return -1;
        }

        /* Re-enable the SD clock */
        control1 = read32(SD_CONTROL1);
        control1 |= (1 << 2);
        write32(SD_CONTROL1, control1);

        delayus(10000);

        /* Check DAT[3:0] */
        status_reg = read32(SD_PRESENT_STS);
        dat30 = (status_reg >> 20) & 0xf;
        if (dat30 != 0xf) {
            warn("DAT[3:0] did not settle to 1111b (%01x)", dat30);
            self->failed_voltage_switch = 1;
            emmc_power_on_off(self, POWER_OFF_MODE, 0);

            return -1;
        }
        trace("voltage switch complete");
    }

    /* Send CMD2 to get the cards CID */
    if (!emmc_issue_command(self, ALL_SEND_CID, 0, 500000)) {
        error("error sending ALL_SEND_CID");
        return -1;
    }
    self->device_id[0] = self->last_r0;
    self->device_id[1] = self->last_r1;
    self->device_id[2] = self->last_r2;
    self->device_id[3] = self->last_r3;
    /*
    debug("card CID: 0x%x, 0x%x, 0x%x, 0x%x", self->device_id[3],
          self->device_id[2], self->device_id[1], self->device_id[0]);
    */

    /* Send CMD3 to enter the data state */
    if (!emmc_issue_command(self, SEND_RELATIVE_ADDR, 0, 500000)) {
        error("error sending SEND_RELATIVE_ADDR");
        return -1;
    }


    uint32_t cmd3_resp = self->last_r0;
    trace("cmd3 response: 0x%x", cmd3_resp);

    self->card_rca = (cmd3_resp >> 16) & 0xffff;
    uint32_t crc_error = (cmd3_resp >> 15) & 0x1;
    uint32_t illegal_cmd = (cmd3_resp >> 14) & 0x1;
    uint32_t error = (cmd3_resp >> 13) & 0x1;
    uint32_t status = (cmd3_resp >> 9) & 0xf;
    uint32_t ready = (cmd3_resp >> 8) & 0x1;

    if (crc_error || illegal_cmd || error || !ready) {
        warn("CMD3 response error");
        return -1;
    }
    //debug("RCA: 0x%x", self->card_rca);


    /* Now select the card (toggles it to transfer state) */
    if (!emmc_issue_command
        (self, SELECT_CARD, self->card_rca << 16, 500000)) {
        error("error sending CMD7");
        return -1;
    }

    uint32_t cmd7_resp = self->last_r0;
    status = (cmd7_resp >> 9) & 0xf;

    if ((status != 3) && (status != 4)) {
        error("invalid status %d", status);
        return -1;
    }

    /* If not an SDHC card, ensure BLOCKLEN is 512 bytes */
    if (!self->card_supports_sdhc) {
        if (!emmc_issue_command(self, SET_BLOCKLEN, SD_BLOCK_SIZE, 500000)) {
            error("error sending SET_BLOCKLEN");
            return -1;
        }
    }

    uint32_t controller_block_size = read32(SD_BLKSIZECNT);
    controller_block_size &= (~0xfff);
    controller_block_size |= 0x200;
    write32(SD_BLKSIZECNT, controller_block_size);

    /* Get the cards SCR register */
    self->buf = &(self->scr.scr[0]);
    self->block_size = 8;
    self->blocks_to_transfer = 1;

    emmc_issue_command(self, SEND_SCR, 0, 1000000);

    self->block_size = SD_BLOCK_SIZE;
    if (FAIL(self)) {
        error("error sending SEND_SCR");
        return -2;
    }

    /* Determine card version */
    /* Note that the SCR is big-endian */
    uint32_t scr0 = be2le32(self->scr.scr[0]);
    self->scr.sd_version = SD_VER_UNKNOWN;
    uint32_t sd_spec = (scr0 >> (56 - 32)) & 0xf;
    uint32_t sd_spec3 = (scr0 >> (47 - 32)) & 0x1;
    uint32_t sd_spec4 = (scr0 >> (42 - 32)) & 0x1;
    self->scr.sd_bus_widths = (scr0 >> (48 - 32)) & 0xf;
    if (sd_spec == 0) {
        self->scr.sd_version = SD_VER_1;
    } else if (sd_spec == 1) {
        self->scr.sd_version = SD_VER_1_1;
    } else if (sd_spec == 2) {
        if (sd_spec3 == 0) {
            self->scr.sd_version = SD_VER_2;
        } else if (sd_spec3 == 1) {
            if (sd_spec4 == 0) {
                self->scr.sd_version = SD_VER_3;
            } else if (sd_spec4 == 1) {
                self->scr.sd_version = SD_VER_4;
            }
        }
    }

    debug("debug: SCR: version %s, bus_widths 0x%x",
          sd_versions[self->scr.sd_version], self->scr.sd_bus_widths);


#ifdef SD_HIGH_SPEED
    /* If card supports CMD6, read switch information from card */
    if (self->scr.sd_version >= SD_VER_1_1) {
        /* 512 bit response */
        uint8_t cmd6_resp[64];
        self->buf = &cmd6_resp[0];
        self->block_size = 64;

        /* CMD6 Mode 0: Check Function (Group 1, Access Mode) */
        if (!emmc_issue_command(self, SWITCH_FUNC, 0x00fffff0, 100000)) {
            error("error sending SWITCH_FUNC (Mode 0)");
        } else {
            /* Check Group 1, Function 1 (High Speed/SDR25) */
            self->card_supports_hs = (cmd6_resp[13] >> 1) & 0x1;

            /* Attempt switch if supported */
            if (self->card_supports_hs) {
              /*
                trace("switching to %s mode",
                      self->card_supports_18v ? "SDR25" : "High Speed");
              */
                /* CMD6 Mode 1: Set Function (Group 1, Access Mode = High Speed/SDR25) */
                if (!emmc_issue_command
                    (self, SWITCH_FUNC, 0x80fffff1, 100000)) {

                    error("failed to switch to %s mode",
                          self->card_supports_18v ? "SDR25" :
                          "High Speed");

                } else {
                    /* Success; switch clock to 50MHz */
                    emmc_switch_clock_rate(self, SD_CLOCK_HIGH);

                    trace("switch to 50MHz clock complete");
                }
            }
        }

        /* Restore block size */
        self->block_size = SD_BLOCK_SIZE;
    }
#endif



    if (self->scr.sd_bus_widths & 4) {
        /* Set 4-bit transfer mode (ACMD6) */
        /* See HCSS 3.4 for the algorithm */
#ifdef SD_4BIT_DATA
        trace("switching to 4-bit data mode");
        /* Disable card interrupt in host */
        uint32_t old_irpt_mask = read32(SD_INT_STS_EN);
        uint32_t new_iprt_mask = old_irpt_mask & ~(1 << 8);
        write32(SD_INT_STS_EN, new_iprt_mask);
        /* Send ACMD6 to change the card's bit mode */
        if (!emmc_issue_command(self, SET_BUS_WIDTH, 2, 500000)) {
            error("failed to switch to 4-bit data mode");
        } else {
            /* Change bit mode for Host */
            uint32_t control0 = read32(SD_CONTROL0);
            control0 |= 0x2;
            write32(SD_CONTROL0, control0);

            /* Re-enable card interrupt in host */
            write32(SD_INT_STS_EN, old_irpt_mask);
            trace("switch to 4-bit complete");
        }
#endif
    }

    debug("info: found valid version %s SD card",
         sd_versions[self->scr.sd_version]);

    /* Reset interrupt register */
    write32(SD_INT_STS, 0xffffffff);

    return 0;
}

/* 成功の場合は1を返す、失敗の場合は0を返す */
static int emmc_issue_command(struct emmc *self, uint32_t cmd, uint32_t arg,
                   int timeout)
{
    debug("emmc_issue_command: cmd=%d, arg=0x%08x", cmd & 0xff, arg);
    /* First, handle any pending interrupts */
    emmc_handle_interrupts(self);

    /* Stop the command issue if it was the card remove interrupt that was handled */
    if (self->card_removal) {
        warn("card has removed");
        self->last_cmd_success = 0;
        return 0;
    }

    /* Now run the appropriate commands by calling IssueCommandInt() */
    if (cmd & IS_APP_CMD) {
        cmd &= 0xff;
        debug("issuing command ACMD%d", cmd);

        if (sd_acommands[cmd] == SD_CMD_RESERVED(0)) {
            error("invalid command ACMD%d", cmd);
            self->last_cmd_success = 0;
            return 0;
        }
        self->last_cmd = APP_CMD;

        uint32_t rca = 0;
        if (self->card_rca) {
            rca = self->card_rca << 16;
        }
        emmc_issue_command_int(self, sd_commands[APP_CMD], rca, timeout);
        debug("cmd: %d (0x%08x), result: %s", sd_commands[APP_CMD] >> 24, rca, self->last_cmd_success ? "success" : "failure");
        if (self->last_cmd_success) {
            self->last_cmd = cmd | IS_APP_CMD;
            emmc_issue_command_int(self, sd_acommands[cmd], arg, timeout);
            debug("cmd: %d (0x%08x), result: %s", sd_acommands[cmd] >> 24, arg, self->last_cmd_success ? "success" : "failure");
        }
    } else {
        if (sd_commands[cmd] == SD_CMD_RESERVED(0)) {
            error("invalid command CMD%d", cmd);
            self->last_cmd_success = 0;
            return 0;
        }

        self->last_cmd = cmd;
        emmc_issue_command_int(self, sd_commands[cmd], arg, timeout);
        debug("cmd: %d (0x%08x), result: %s", sd_commands[cmd] >> 24, arg, self->last_cmd_success ? "success" : "failure");
    }
    return self->last_cmd_success;
}

#endif
