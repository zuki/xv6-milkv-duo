#include "sdhci.h"
#include "mmc.h"
#include "types.h"
#include "printf.h"
#include "io.h"
#include "bitops.h"
#include "errno.h"

static struct sdhci_host sdhci_host;

static int sdhci_setup_cfg(struct mmc_config *cfg, struct sdhci_host *host,
        uint32_t f_max, uint32_t f_min)
{
    uint32_t caps, caps_1 = 0;
    caps = read32(SD0 + SDHCI_CAPABILITIES);
    debug("%s, caps: 0x%x", __func__, caps);

    if ((caps & SDHCI_CAN_DO_SDMA)) {
        host->flags |= USE_SDMA;
    } else {
        debug("%s: Your controller doesn't support SDMA!!",
              __func__);
    }

    if (host->quirks & SDHCI_QUIRK_REG32_RW)
        host->version =
            read32(SD0 + (SDHCI_HOST_VERSION - 2)) >> 16;
    else
        host->version = read16(SD0 + SDHCI_HOST_VERSION);

    cfg->name = host->name;

    /* Check whether the clock multiplier is supported or not */
    if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
        caps_1 = read32(SD0 + SDHCI_CAPABILITIES_1);
        debug("%s, caps_1: 0x%x", __func__, caps_1);
        host->clk_mul = (caps_1 & SDHCI_CLOCK_MUL_MASK) >>
                SDHCI_CLOCK_MUL_SHIFT;
    }

    if (host->max_clk == 0) {
        if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300)
            host->max_clk = (caps & SDHCI_CLOCK_V3_BASE_MASK) >>
                SDHCI_CLOCK_BASE_SHIFT;
        else
            host->max_clk = (caps & SDHCI_CLOCK_BASE_MASK) >>
                SDHCI_CLOCK_BASE_SHIFT;
        host->max_clk *= 1000000;
        if (host->clk_mul)
            host->max_clk *= host->clk_mul;
    }
    if (host->max_clk == 0) {
        info("%s: Hardware doesn't specify base clock frequency",
               __func__);
        return -EINVAL;
    }
    if (f_max && (f_max < host->max_clk))
        cfg->f_max = f_max;
    else
        cfg->f_max = host->max_clk;
    if (f_min)
        cfg->f_min = f_min;
    else {
        if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300)
            cfg->f_min = cfg->f_max / SDHCI_MAX_DIV_SPEC_300;
        else
            cfg->f_min = cfg->f_max / SDHCI_MAX_DIV_SPEC_200;
    }
    cfg->voltages = 0;
    if (caps & SDHCI_CAN_VDD_330)
        cfg->voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
    if (caps & SDHCI_CAN_VDD_300)
        cfg->voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
    if (caps & SDHCI_CAN_VDD_180)
        cfg->voltages |= MMC_VDD_165_195;

    if (host->quirks & SDHCI_QUIRK_BROKEN_VOLTAGE)
        cfg->voltages |= host->voltages;

    if (caps & SDHCI_CAN_DO_HISPD)
        cfg->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz;

    cfg->host_caps |= MMC_MODE_4BIT;

    /* Since Host Controller Version3.0 */
    if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
        if (!(caps & SDHCI_CAN_DO_8BIT))
            cfg->host_caps &= ~MMC_MODE_8BIT;
    }

    if (host->quirks & SDHCI_QUIRK_BROKEN_HISPD_MODE) {
        cfg->host_caps &= ~MMC_MODE_HS;
        cfg->host_caps &= ~MMC_MODE_HS_52MHz;
    }

    if (!(cfg->voltages & MMC_VDD_165_195) ||
        (host->quirks & SDHCI_QUIRK_NO_1_8_V))
        caps_1 &= ~(SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
                SDHCI_SUPPORT_DDR50);

    if (host->quirks & SDHCI_QUIRK_NO_1_8_V)
        caps_1 &= ~(SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
                SDHCI_SUPPORT_DDR50);

    if (caps_1 & (SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
              SDHCI_SUPPORT_DDR50))
        cfg->host_caps |= MMC_CAP(UHS_SDR12) | MMC_CAP(UHS_SDR25);

    if (caps_1 & SDHCI_SUPPORT_SDR104) {
        cfg->host_caps |= MMC_CAP(UHS_SDR104) | MMC_CAP(UHS_SDR50);
        /*
         * SD3.0: SDR104 is supported so (for eMMC) the caps2
         * field can be promoted to support HS200.
         */
        cfg->host_caps |= MMC_CAP(MMC_HS_200);
    } else if (caps_1 & SDHCI_SUPPORT_SDR50) {
        cfg->host_caps |= MMC_CAP(UHS_SDR50);
    }

    if (caps_1 & SDHCI_SUPPORT_DDR50)
        cfg->host_caps |= MMC_CAP(UHS_DDR50);

    if (host->host_caps)
        cfg->host_caps |= host->host_caps;

    cfg->b_max = 65535; // CONFIG_SYS_MMC_MAX_BLK_COUNTが未定義の場合のデフォルト

    return 0;
}

static void cvi_sdio0_pad_function(bool bunplug)
{
    /* Name                unplug plug
     * PAD_SDIO0_CD        SDIO0  SDIO0
     * PAD_SDIO0_PWR_EN    SDIO0  SDIO0
     * PAD_SDIO0_CLK       XGPIO  SDIO0
     * PAD_SDIO0_CMD       XGPIO  SDIO0
     * PAD_SDIO0_D0        XGPIO  SDIO0
     * PAD_SDIO0_D1        XGPIO  SDIO0
     * PAD_SDIO0_D2        XGPIO  SDIO0
     * PAD_SDIO0_D3        XGPIO  SDIO0
     * 0x0: SDIO0 function
     * 0x3: XGPIO function
     */

    uint8_t val = (bunplug) ? 0x3 : 0x0;

    write32(PAD_SDIO0_CD_REG, 0x0);
    write32(PAD_SDIO0_PWR_EN_REG, 0x0);
    write32(PAD_SDIO0_CLK_REG, val);
    write32(PAD_SDIO0_CMD_REG, val);
    write32(PAD_SDIO0_D0_REG, val);
    write32(PAD_SDIO0_D1_REG, val);
    write32(PAD_SDIO0_D2_REG, val);
    write32(PAD_SDIO0_D3_REG, val);
}


static void cvi_sdio0_pad_setting(bool reset)
{
    if (reset) {
        clrsetbits32(REG_SDIO0_PWR_EN_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_PWR_EN_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_CD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CD_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_CLK_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CLK_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_CMD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CMD_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT1_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT1_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT0_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT0_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT2_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT2_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT3_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT3_PAD_RESET << REG_SDIO0_PAD_SHIFT);

    } else {
        clrsetbits32(REG_SDIO0_PWR_EN_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_PWR_EN_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_CD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CD_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_CLK_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CLK_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_CMD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CMD_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT1_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT1_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT0_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT0_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT2_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT2_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        clrsetbits32(REG_SDIO0_DAT3_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT3_PAD_VALUE << REG_SDIO0_PAD_SHIFT);
    }
}

static void sdhci_reset(struct sdhci_host *host, uint8_t mask)
{
    unsigned long timeout;
    uint16_t ctrl_2;

    // 1. cmd/data lineのソフトウェアリセット
    timeout = 100;
    write8(SD0 + SDHCI_SOFTWARE_RESET, mask);
    while (read8(SD0 + SDHCI_SOFTWARE_RESET) & mask) {
        if (timeout == 0) {
            info("%s: Reset 0x%x never completed.",
                   __func__, (int)mask);
            return;
        }
        timeout--;
        delayus(1000);
    }

    // 以下はcv180x独自

    ctrl_2 = read16(SD0 + SDHCI_HOST_CONTROL2);
    debug("%s-%d MMC%d : ctrl_2 = 0x%04x", __func__, __LINE__, host->index, ctrl_2);

    // 3. UHS_MODE_SELを抽出して処理
    ctrl_2 &= SDHCI_CTRL_UHS_MASK;
    if (ctrl_2 == SDHCI_CTRL_UHS_SDR104) {  // SDR104: 最速
        //reg_0x200[1] = 0: Latency 1t for cmd in をdisable
        write32(SD0 + CVI_SDHCI_VENDOR_OFFSET, read32(SD0 + CVI_SDHCI_VENDOR_OFFSET) & ~(BIT(1)));
        //reg_0x200[16] = 1 for sd1: timer clock source = 32K
        write32(SD0 + CVI_SDHCI_VENDOR_OFFSET, read32(SD0 + CVI_SDHCI_VENDOR_OFFSET) | BIT(16));
        //reg_0x24c[0] = 0: PHY tx data pipe enable
        write32(SD0 + CVI_SDHCI_PHY_CONFIG, read32(SD0 + CVI_SDHCI_PHY_CONFIG) & ~(BIT(0)));
        //reg_0x240[22:16] = tap reg_0x240[9:8] = 1 reg_0x240[6:0] = 0
        write32(SD0 + CVI_SDHCI_PHY_TX_RX_DLY, (BIT(8) | ((0 & 0x7F) << 16)));
    } else {    // SDR104以外
        // DS/HSのリセット
        //reg_0x200[1] = 1
        write32(SD0 + CVI_SDHCI_VENDOR_OFFSET, read32(SD0 + CVI_SDHCI_VENDOR_OFFSET) | BIT(1));
        //reg_0x200[16] = 1 for sd1
        write32(SD0 + CVI_SDHCI_VENDOR_OFFSET, read32(SD0 + CVI_SDHCI_VENDOR_OFFSET) | BIT(16));
        //reg_0x24c[0] = 1: bypass
        write32(SD0 + CVI_SDHCI_PHY_CONFIG, read32(SD0 + CVI_SDHCI_PHY_CONFIG) | BIT(0));
        //reg_0x240[25:24] = 1 reg_0x240[22:16] = 0 reg_0x240[9:8] = 1 reg_0x240[6:0] = 0
        write32(SD0 + CVI_SDHCI_PHY_TX_RX_DLY, 0x1000100);
    }

    debug("reg_0x24C = 0x%08x, reg_200 = 0x%08x",
         read32(SD0 + CVI_SDHCI_PHY_CONFIG),
         read32(SD0 + CVI_SDHCI_VENDOR_OFFSET));
    debug("reg_0x240 = 0x%08x, reg_0x248 = 0x%08x",
         read32(SD0 + CVI_SDHCI_PHY_TX_RX_DLY),
         read32(SD0 + CVI_SDHCI_PHY_DLY_STS));

}

// カードの挿入をチェック
int sdhci_get_cd(struct sdhci_host *host)
{
    uint32_t reg;

    reg = read32(SD0 + SDHCI_PRESENT_STATE);
    debug("%s reg = 0x08%x", __func__, reg);
    if (reg & SDHCI_CARD_PRESENT) {
        return 1;
    } else {
        return 0;
    }
}

static void sdhci_set_power(unsigned short power)
{
    uint8_t pwr = 0;

    // 1. 電圧設定
    if (power != (unsigned short)-1) {
        switch (1 << power) {
        case MMC_VDD_165_195:
            pwr = SDHCI_POWER_180;
            break;
        case MMC_VDD_29_30:
        case MMC_VDD_30_31:
            pwr = SDHCI_POWER_300;
            break;
        case MMC_VDD_32_33:
        case MMC_VDD_33_34:
            pwr = SDHCI_POWER_330;
            break;
        }
    }

    // 2. power=0xFFの場合はパワーオフにして復帰
    if (pwr == 0) {
        write8(SD0 + SDHCI_POWER_CONTROL, 0);
        return;
    }

    // 3. 電源オン
    pwr |= SDHCI_POWER_ON;

    // 1,3を書き込んで有効あ
    write8(SD0 + SDHCI_POWER_CONTROL, pwr);
}

static void sdhci_host_init(struct sdhci_host *host)
{
    // CV180X固有の初期化
    // 1. カード挿入の確認
    int present = sdhci_get_cd(host);
    // 2. SDのsystemレベルの電源設定
    if (present == 1) {
        // Voltage switching flow (3.5)
        //(reg_pwrsw_auto=1, reg_pwrsw_disc=0, reg_pwrsw_vsel=0(3.0v), reg_en_pwrsw=1)
        write32(MMIO_BASE + REG_TOP_SD_PWRSW_CTRL, 0x9);
        cvi_sdio0_pad_function(false);
        cvi_sdio0_pad_setting(false);
    } else {
        // Voltage close flow
        //(reg_pwrsw_auto=1, reg_pwrsw_disc=1, reg_pwrsw_vsel=1(1.8v), reg_en_pwrsw=0)
        write32(MMIO_BASE + REG_TOP_SD_PWRSW_CTRL, 0xE);
        cvi_sdio0_pad_function(true);
        cvi_sdio0_pad_setting(true);
    }

    // sdhci共通の初期化
    // 1. リセット
    sdhci_reset(host, SDHCI_RESET_ALL);

    // 2. カード挿入の確認
    present = sdhci_get_cd(host);
    // 3. 電源電圧の選択
    if (present == 1) {
        sdhci_set_power(generic_fls(host->cfg.voltages) - 1);
        delayms(5);
    } else if (present == 0) {      // 挿入されていない場合は電源オフ
        sdhci_set_power((unsigned short)-1);
        warn("sdhci power off");
        delayms(30);
    }

    // 4. 割り込みを有効化（SDコントローラが提供している割り込みのみ）
    write32(SD0 + SDHCI_INT_ENABLE, SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK);
    // 5. 割り込みは有効にするが割り込み信号は出さない
    write32(SD0 + SDHCI_SIGNAL_ENABLE, 0x0);

    debug("set IP clock to 375Mhz");
    write32(SDHCI_PLL_REG, MMC_MAX_CLOCK_DIV_VALUE);

    debug("Be sure to switch clock source to PLL");
    clrbits32(CLOCK_BYPASS_SELECT_REGISTER, BIT(SDHCI_PLL_INDEX));
    debug("XTAL->PLL reg = 0x%x", read32(CLOCK_BYPASS_SELECT_REGISTER));

    debug("eMMC/SD CLK is %d in FPGA_ASIC", host->max_clk);

    if (SDHCI_IS_64_ADDRESSING) {
        write16(SD0 + SDHCI_HOST_CONTROL2, read16(SD0 + SDHCI_HOST_CONTROL2) | SDHCI_HOST_VER4_ENABLE | SDHCI_HOST_ADDRESSING);
    }

    /* Default value */
    if (SDHCI_RESET_TX_RX_PHY) {
        write32(SD0 + CVI_SDHCI_VENDOR_OFFSET, 2);
        write32(SD0 + CVI_SDHCI_PHY_TX_RX_DLY, 0x01000100);
        write32(SD0 + SDHCI_PHY_CONFIG, 00000001);
    }
}

// TODO: mmc.cを書く際に、sdhci_set_voltage(), sdhci_execute_tuning()を実装すること
int sdhci_init(void)
{
    struct sdhci_host *host = &sdhci_host;

    host->name = SDHCI_NAME;
    host->ioaddr = (void *)SDHCI_IOADDR;  // SD0の基底アドレス
    host->bus_width = SDHCI_BUS_WIDTH;
    host->max_clk = SDHCI_MAX_CLK;
    if (SDHCI_NO_1_8_V)
        host->quirks |= SDHCI_QUIRK_NO_1_8_V;

    int ret = sdhci_setup_cfg(&host->cfg, host, SDHCI_MMC_FMAX_FREQ, SDHCI_MMC_FMIN_FREQ);
    if (ret) {
        error("sdhci_init: failed sdhci_setup_cfg: %n", ret);
        return ret;
    }

    sdhci_host_init(host);

    // FIXME: mmc_create()はここでする必要があるか?
    info("sdhci_init ok");

    return 0;
}
