#include "sdhci.h"
#include "mmc.h"
#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "printf.h"
#include "io.h"
#include "bitops.h"
#include "errno.h"

static struct sdhci_host sdhci_host;

static void sdhci_dump_cfg(struct mmc_config *cfg) {
    printf("mmc_config: name: %s", cfg->name);
    printf(" caps: 0x%08x", cfg->host_caps);
    printf(" volt: 0x%08x\n", cfg->voltages);
    printf("            fmin: 0x%08x", cfg->f_min);
    printf(" fmax: 0x%08x", cfg->f_max);
    printf(" bmax: 0x%08x", cfg->b_max);
    printf(" type: 0x%02x\n", cfg->part_type);
}

static inline uint32_t CHECK_MASK_BIT(void *_mask, uint32_t bit)
{
    uint32_t w = bit / 8;
    uint32_t off = bit % 8;

    return ((uint8_t *)_mask)[w] & (1 << off);
}

static inline void SET_MASK_BIT(void *_mask, uint32_t bit)
{
    uint32_t byte = bit / 8;
    uint32_t offset = bit % 8;
    ((uint8_t *)_mask)[byte] |= (1 << offset);
}

static void reset_after_tuning_pass(struct sdhci_host *host)
{
    debug("tuning pass");

    /* Clear BUF_RD_READY intr */
    write16(SD0 + SDHCI_INT_STATUS, read16(SD0 + SDHCI_INT_STATUS) & (~(0x1 << 5)));

    /* Set SDHCI_SOFTWARE_RESET.SW_RST_DAT = 1 to clear buffered tuning block */
    write8(SD0 + SDHCI_SOFTWARE_RESET, read8(SD0 + SDHCI_SOFTWARE_RESET) | (0x1 << 2));

    /* Set SDHCI_SOFTWARE_RESET.SW_RST_CMD = 1    */
    write8(SD0 + SDHCI_SOFTWARE_RESET, read8(SD0 + SDHCI_SOFTWARE_RESET) | (0x1 << 1));

    while (read8(SD0 + SDHCI_SOFTWARE_RESET) & 0x3)
        ;
}

static int sdhci_setup_cfg(struct mmc_config *cfg, struct sdhci_host *host,
        uint32_t f_max, uint32_t f_min)
{
    uint32_t caps, caps_1 = 0;
    // 1. カードのケーパビリティを取得
    caps = read32(SD0 + SDHCI_CAPABILITIES);
    debug("%s, caps: 0x%x", __func__, caps);

    // 2. SDMAに対応しているか
    if ((caps & SDHCI_CAN_DO_SDMA)) {
        host->flags |= USE_SDMA;
    } else {
        debug("%s: Your controller doesn't support SDMA!!",
              __func__);
    }

    // 2. SD Hostのバージョンを取得
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

#if 1
    sdhci_dump_cfg(cfg);
#endif

    return 0;
}

// PAD_SDIO0_X をSDIOに使うか(false), GPIOに使う(trueか
static void cvi_sdio0_pad_function(bool unplug)
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

    uint8_t val = (unplug) ? 0x3 : 0x0;

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

static void cvi_mmc_set_tap(struct sdhci_host *host, uint16_t tap)
{
    trace("%d", tap);
    // Set sd_clk_en(0x2c[2]) to 0
    write16(SD0 + SDHCI_CLOCK_CONTROL, read16(SD0 + SDHCI_CLOCK_CONTROL) & (~(0x1 << 2)));
    write32(SD0 + CVI_SDHCI_VENDOR_OFFSET, 0);
    write32(SD0 + CVI_SDHCI_PHY_TX_RX_DLY, BIT(8) | tap << 16);
    write32(SD0 + CVI_SDHCI_PHY_CONFIG, 0);
    // Set sd_clk_en(0x2c[2]) to 1
    write16(SD0 + SDHCI_CLOCK_CONTROL, read16(SD0 + SDHCI_CLOCK_CONTROL) | (0x1 << 2));
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
            info("Reset 0x%x never completed.", (int)mask);
            return;
        }
        timeout--;
        delayus(1000);
    }

    // 以下はcv180x独自

    ctrl_2 = read16(SD0 + SDHCI_HOST_CONTROL2);
    debug("MMC%d : ctrl_2 = 0x%04x", host->index, ctrl_2);

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

    debug("PHY_CONFIG: 0x%08x, VENDOR_OFFSET: 0x%08x",
         read32(SD0 + CVI_SDHCI_PHY_CONFIG),
         read32(SD0 + CVI_SDHCI_VENDOR_OFFSET));
    debug("PHY_TX_RX_DLY: 0x%08x, PHY_DLY_STS: 0x%08x",
         read32(SD0 + CVI_SDHCI_PHY_TX_RX_DLY),
         read32(SD0 + CVI_SDHCI_PHY_DLY_STS));

}

// カードの挿入をチェック
int sdhci_get_cd(struct sdhci_host *host)
{
    uint32_t reg;

    reg = read32(SD0 + SDHCI_PRESENT_STATE);
    debug("SDHCI_PRESENT_STATE: 0x%08x", reg);
    if (reg & SDHCI_CARD_PRESENT) {
        debug("sd present");
        return 1;
    } else {
        debug("sd not present");
        return 0;
    }
}

// SD バスパワーの設定: power=0xff の場合はパワーオフ
static void sdhci_set_power(unsigned short power)
{
    uint8_t pwr = 0;

    // 1. 電圧設定
    if (power != (unsigned short)-1) {
        switch (1 << power) {
        case MMC_VDD_165_195:
            debug("set 1.8");
            pwr = SDHCI_POWER_180;
            break;
        case MMC_VDD_29_30:
        case MMC_VDD_30_31:
            debug("set 3.0");
            pwr = SDHCI_POWER_300;
            break;
        case MMC_VDD_32_33:
        case MMC_VDD_33_34:
            debug("set 3.3");
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

    // 1,3を書き込んで有効化
    write8(SD0 + SDHCI_POWER_CONTROL, pwr);
}

// sdhciホストの初期化
static void sdhci_host_init(struct sdhci_host *host)
{
    // CV180X固有の初期化
    // 1. カード挿入の確認
    int present = sdhci_get_cd(host);
    // 2. SD power switchを設定
    if (present == 1) { // SDとし使用
        // Voltage switching flow (3.5.3.2)
        // pwrsw: auto[3]=1, disc[2]=0, vsel[1]=0(3.0v), enable[0]=1
        write32(MMIO_BASE + REG_TOP_SD_PWRSW_CTRL, 0x9);
        cvi_sdio0_pad_function(false);
        cvi_sdio0_pad_setting(false);
    } else {            // GPIOとして使用
        // Voltage close flow
        // pwrsw auto[3]=1, disc[2]=1, vsel[1]=1(1.8v), enable[0]=0
        write32(MMIO_BASE + REG_TOP_SD_PWRSW_CTRL, 0xE);
        cvi_sdio0_pad_function(true);
        cvi_sdio0_pad_setting(true);
    }

    // 3. sdhci共通の初期化
    // 3.1 CMD/DATラインのリセット
    debug("1: call sdhci_reset");
    sdhci_reset(host, SDHCI_RESET_ALL);

    // 3.2 カード挿入の確認
    present = sdhci_get_cd(host);
    debug("2: get_cd: %d", present)
    // 3.3 電源電圧の選択
    debug("3: set_power");
    if (present == 1) { // 電圧を選択してバスパワーをオン
        sdhci_set_power(generic_fls(host->cfg.voltages) - 1);
        delayms(5);
    } else if (present == 0) {      // 挿入されていない場合は電源オフ
        sdhci_set_power((unsigned short)-1);
        warn("sdhci power off");
        delayms(30);
    }

    // 3.4 割り込みを有効化（SDコントローラが提供している割り込みのみ）
    write32(SD0 + SDHCI_INT_ENABLE, SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK);
    debug("4: int_enable: 0x%0x8x", read32(SD0 + SDHCI_INT_ENABLE));
    // 3.5 割り込みは有効にするが割り込み信号は出さない
    write32(SD0 + SDHCI_SIGNAL_ENABLE, 0x0);
    debug("5: signal_enable: 0x%0x8x", read32(SD0 + SDHCI_SIGNAL_ENABLE));

    // 3.6 PLLの設定
    write32(SDHCI_PLL_REG, MMC_MAX_CLOCK_DIV_VALUE);
    debug("6: set max_clock_div_value: 0x%0x8x", read32(SDHCI_PLL_REG));

    // 3.7 XtalをSD clkに使用する
    clrbits32(CLOCK_BYPASS_SELECT_REGISTER, BIT(SDHCI_PLL_INDEX));
    debug("7: switch clock source for SD clk to xtal: 0x%08x", read32(CLOCK_BYPASS_SELECT_REGISTER));

    debug("eMMC/SD CLK is %d in FPGA_ASIC", host->max_clk);

    // 3.8 64-bitアドレッシングを設定: bit[12-13]は仕様書に情報なし
    if (SDHCI_IS_64_ADDRESSING) {
        write16(SD0 + SDHCI_HOST_CONTROL2, read16(SD0 + SDHCI_HOST_CONTROL2) | SDHCI_HOST_VER4_ENABLE | SDHCI_HOST_ADDRESSING);
        debug("8: host_cntl2: 0x%04x", read16(SD0 + SDHCI_HOST_CONTROL2));
    }

    /* 3.9 Default value */
    if (SDHCI_RESET_TX_RX_PHY) {
        write32(SD0 + CVI_SDHCI_VENDOR_OFFSET, 2);
        write32(SD0 + CVI_SDHCI_PHY_TX_RX_DLY, 0x01000100);
        write32(SD0 + CVI_SDHCI_PHY_CONFIG, 00000001);
    }
}

int sdhci_execute_tuning(struct mmc *mmc, uint8_t opcode)
{
    uint16_t min = 0;
    uint32_t k = 0;
    int32_t ret;
    uint32_t retry_cnt = 0;

    uint32_t tuning_result[4] = {0, 0, 0, 0};
    uint32_t rx_lead_lag_result[4] = {0, 0, 0, 0};
    char tuning_graph[TUNE_MAX_PHCODE + 1];
    char rx_lead_lag_graph[TUNE_MAX_PHCODE + 1];

    uint32_t reg = 0;
    uint32_t reg_rx_lead_lag = 0;
    int32_t max_lead_lag_idx = -1;
    int32_t max_window_idx = -1;
    int32_t cur_window_idx = -1;
    uint16_t max_lead_lag_size = 0;
    uint16_t max_window_size = 0;
    uint16_t cur_window_size = 0;
    int32_t rx_lead_lag_phase = -1;
    int32_t final_tap = -1;
    uint32_t rate = 0;

    struct sdhci_host *host = mmc->priv;

    uint32_t norm_stat_en_b, err_stat_en_b;
    uint32_t norm_signal_en_b, ctl2;

    norm_stat_en_b = read16(SD0 + SDHCI_INT_ENABLE);
    err_stat_en_b = read16(SD0 + SDHCI_ERR_INT_STATUS_EN);
    norm_signal_en_b = read32(SD0 + SDHCI_SIGNAL_ENABLE);

    reg = read16(SD0 + SDHCI_ERR_INT_STATUS);
    debug("1. mmc%d : SDHCI_ERR_INT_STATUS: 0x%08x", host->index, reg);

    reg = read16(SD0 + SDHCI_HOST_CONTROL2);
    debug("2. mmc%d : host ctrl2: 0x%x", host->index, reg);
    /* Set Host_CTRL2_R.SAMPLE_CLK_SEL=0 */
    write16(SD0 + SDHCI_HOST_CONTROL2,
             read16(SD0 + SDHCI_HOST_CONTROL2) & (~(0x1 << 7)));
    write16(SD0 + SDHCI_HOST_CONTROL2,
             read16(SD0 + SDHCI_HOST_CONTROL2) & (~(0x3 << 4)));

    reg = read16(SD0 + SDHCI_HOST_CONTROL2);
    debug("3. mmc%d : host ctrl2: 0x%x", host->index, reg);

    while (min < TUNE_MAX_PHCODE) {
        retry_cnt = 0;
        write16(SD0 + CVI_SDHCI_VENDOR_OFFSET, BIT(2));
        cvi_mmc_set_tap(host, min);
        reg_rx_lead_lag = read16(SD0 + CVI_SDHCI_PHY_DLY_STS) & BIT(1);

retry_tuning:
        ret = mmc_send_tuning(mmc, opcode, NULL);

        if (!ret && retry_cnt < MAX_TUNING_CMD_RETRY_COUNT) {
            //debug("retry: %d, ret: %d", retry_cnt, ret);
            retry_cnt++;
            goto retry_tuning;
        }

        if (ret) {
            SET_MASK_BIT(tuning_result, min);
        }

        if (reg_rx_lead_lag) {
            SET_MASK_BIT(rx_lead_lag_result, min);
        }

        min++;
    }

    reset_after_tuning_pass(host);

    debug("mmc%d : tuning result:      0x%08x 0x%08x 0x%08x 0x%08x", host->index,
         tuning_result[0], tuning_result[1],
         tuning_result[2], tuning_result[3]);
    debug("mmc%d : rx_lead_lag result: 0x%08x 0x%08x 0x%08x 0x%08x", host->index,
         rx_lead_lag_result[0], rx_lead_lag_result[1],
         rx_lead_lag_result[2], rx_lead_lag_result[3]);
    for (k = 0; k < TUNE_MAX_PHCODE; k++) {
        if (CHECK_MASK_BIT(tuning_result, k) == 0)
            tuning_graph[k] = '-';
        else
            tuning_graph[k] = 'x';
        if (CHECK_MASK_BIT(rx_lead_lag_result, k) == 0)
            rx_lead_lag_graph[k] = '0';
        else
            rx_lead_lag_graph[k] = '1';
    }
    tuning_graph[TUNE_MAX_PHCODE] = '\0';
    rx_lead_lag_graph[TUNE_MAX_PHCODE] = '\0';

    debug("mmc%d : tuning graph:      %s", host->index, tuning_graph);
    debug("mmc%d : rx_lead_lag graph: %s", host->index, rx_lead_lag_graph);

    // Find a final tap as median of maximum window
    for (k = 0; k < TUNE_MAX_PHCODE; k++) {
        if (CHECK_MASK_BIT(tuning_result, k) == 0) {
            if (-1 == cur_window_idx) {
                cur_window_idx = k;
            }
            cur_window_size++;

            if (cur_window_size > max_window_size) {
                max_window_size = cur_window_size;
                max_window_idx = cur_window_idx;
                if (max_window_size >= TAP_WINDOW_THLD)
                    final_tap = cur_window_idx + (max_window_size / 2);
            }
        } else {
            cur_window_idx = -1;
            cur_window_size = 0;
        }
    }

    cur_window_idx = -1;
    cur_window_size = 0;
    for (k = 0; k < TUNE_MAX_PHCODE; k++) {
        if (CHECK_MASK_BIT(rx_lead_lag_result, k) == 0) {
            //from 1 to 0 and window_size already computed.
            if (rx_lead_lag_phase == 1 && cur_window_size > 0) {
                max_lead_lag_idx = cur_window_idx;
                max_lead_lag_size = cur_window_size;
                break;
            }
            if (cur_window_idx == -1) {
                cur_window_idx = k;
            }
            cur_window_size++;
            rx_lead_lag_phase = 0;
        } else {
            rx_lead_lag_phase = 1;
            if (cur_window_idx != -1 && cur_window_size > 0) {
                cur_window_size++;
                max_lead_lag_idx = cur_window_idx;
                max_lead_lag_size = cur_window_size;
            } else {
                cur_window_size = 0;
            }
        }
    }
    rate = max_window_size * 100 / max_lead_lag_size;
    debug("mmc%d : MaxWindow[Idx, Width]:[%d,%u] Tuning Tap: %d",
         host->index, max_window_idx, max_window_size, final_tap);
    debug("mmc%d : RX_LeadLag[Idx, Width]:[%d,%u] rate = %d",
         host->index, max_lead_lag_idx, max_lead_lag_size, rate);

    cvi_mmc_set_tap(host, final_tap);
    //cvi_host->final_tap = final_tap;
    ret = mmc_send_tuning(mmc, opcode, NULL);
    debug("mmc%d : finished tuning, code:%d", host->index, final_tap);

    ctl2 = read16(SD0 + SDHCI_HOST_CONTROL2);
    ctl2 &= ~SDHCI_CTRL_EXEC_TUNING;
    write16(SD0 + SDHCI_HOST_CONTROL2, ctl2);

    write16(SD0 + SDHCI_INT_ENABLE, norm_stat_en_b);
    write32(SD0 + SDHCI_SIGNAL_ENABLE, norm_signal_en_b);
    write16(SD0 + SDHCI_ERR_INT_STATUS_EN, err_stat_en_b);

    return ret;
}

static int sdhci_set_clock(struct mmc *mmc, unsigned int clock)
{
    struct sdhci_host *host = mmc->priv;
    unsigned int div, clk = 0, timeout;

    debug("mmc%d : Set clock %d, host->max_clk %d", host->index, clock, host->max_clk);
    /* Wait max 20 ms */
    timeout = 200;
    uint32_t state = read32(SD0 + SDHCI_PRESENT_STATE);
    debug("state: 0x%08x", state);
    while (state & (SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT)) {
        if (timeout == 0) {
            debug("Timeout to wait cmd & data inhibit");
            return -EBUSY;
        }
        timeout--;
        delayus(100);
        state = read32(SD0 + SDHCI_PRESENT_STATE);
    }

    write16(SD0 + SDHCI_CLOCK_CONTROL, 0);

    if (clock == 0)
        return 0;

    if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
        /*
         * Check if the Host Controller supports Programmable Clock
         * Mode.
         */
        if (host->clk_mul) {
            for (div = 1; div <= 1024; div++) {
                if ((host->max_clk / div) <= clock)
                    break;
            }

            /*
             * Set Programmable Clock Mode in the Clock
             * Control register.
             */
            clk = SDHCI_PROG_CLOCK_MODE;
            div--;
        } else {
            /* Version 3.00 divisors must be a multiple of 2. */
            if (host->max_clk <= clock) {
                div = 1;
            } else {
                for (div = 2;
                     div < SDHCI_MAX_DIV_SPEC_300;
                     div += 2) {
                    if ((host->max_clk / div) <= clock)
                        break;
                }
            }
            div >>= 1;
        }
    } else {
        /* Version 2.00 divisors must be a power of 2. */
        for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) {
            if ((host->max_clk / div) <= clock)
                break;
        }
        div >>= 1;
    }

    debug("mmc%d : clk div 0x%x", host->index, div);

    clk |= (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
    clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
        << SDHCI_DIVIDER_HI_SHIFT;
    clk |= SDHCI_CLOCK_INT_EN;

    debug("mmc%d : 0x2c clk reg 0x%x", host->index, clk);

    write16(SD0 + SDHCI_CLOCK_CONTROL, clk);

    /* Wait max 20 ms */
    timeout = 20;
    clk = read16(SD0 + SDHCI_CLOCK_CONTROL);
    while (!(clk & SDHCI_CLOCK_INT_STABLE)) {
        debug("clk: 0x%08x", clk);
        if (timeout == 0) {
            debug("Internal clock never stabilised.");
            return -EBUSY;
        }
        timeout--;
        delayus(1000);
        clk = read16(SD0 + SDHCI_CLOCK_CONTROL);
    }

    clk |= SDHCI_CLOCK_CARD_EN;
    write16(SD0 + SDHCI_CLOCK_CONTROL, clk);
    return 0;
}

// 電源電圧を1.8vに変更すする
static void cvi_sd_voltage_switch(struct mmc *mmc)
{
    // 1. enable SDIO0_CLK[7:5] to set CLK max strengh: 仕様書に情報なし
    setbits32(REG_SDIO0_CLK_PAD_REG, BIT(7) | BIT(6) | BIT(5));

    //Voltage switching flow (1.8v)
    //reg_pwrsw_auto=1, reg_pwrsw_disc=0, pwrsw_vsel=1(1.8v), reg_en_pwrsw=1
    write32(MMIO_BASE + REG_TOP_SD_PWRSW_CTRL, 0xB);

    //wait 1ms
    delayms(1);
}

// UHSスピードモードを設定する
static void sdhci_set_uhs_timing(struct mmc *mmc)
{
    uint32_t reg;

    reg = read16(SD0 + SDHCI_HOST_CONTROL2);
    reg &= ~SDHCI_CTRL_UHS_MASK;

    switch (mmc->selected_mode) {
    case UHS_SDR50:
    case MMC_HS_52:
        reg |= SDHCI_CTRL_UHS_SDR50;
        break;
    case UHS_DDR50:
    case MMC_DDR_52:
        reg |= SDHCI_CTRL_UHS_DDR50;
        break;
    case UHS_SDR104:
    case MMC_HS_200:
        reg |= SDHCI_CTRL_UHS_SDR104;
        break;
    case MMC_HS_400:
        reg |= SDHCI_CTRL_HS400;
        break;
    default:
        reg |= SDHCI_CTRL_UHS_SDR12;
    }

    write16(SD0 + SDHCI_HOST_CONTROL2, reg);
    delayms(5);
    debug("USH speed mode: %d", read8(SD0 + SDHCI_HOST_CONTROL2) & 0x07);
}

// 転送データ幅、スピードをセットする
int sdhci_set_ios(struct mmc *mmc)
{
    uint32_t ctrl;
    struct sdhci_host *host = mmc->priv;
    bool no_hispd_bit = false;

    // 1. 必要であればclkをセット(on->off/off->onの場合い)
    if (mmc->clk_disable)
        sdhci_set_clock(mmc, 0);            // mmcのクロックをオフ
    else if (mmc->clock != host->clock)
        sdhci_set_clock(mmc, mmc->clock);

    /// 2. バス幅をセット
    ctrl = read8(SD0 + SDHCI_HOST_CONTROL);
    debug("org host_contorl: 0x%08x", ctrl);
    if (mmc->bus_width == 8) {
        ctrl &= ~SDHCI_CTRL_4BITBUS;
        if ((SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) ||
                (host->quirks & SDHCI_QUIRK_USE_WIDE8))
            ctrl |= SDHCI_CTRL_8BITBUS;
    } else {
        if ((SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) ||
            (host->quirks & SDHCI_QUIRK_USE_WIDE8))
        ctrl &= ~SDHCI_CTRL_8BITBUS;
        if (mmc->bus_width == 4)
            ctrl |= SDHCI_CTRL_4BITBUS;
        else
            ctrl &= ~SDHCI_CTRL_4BITBUS;
    }

    if ((host->quirks & SDHCI_QUIRK_NO_HISPD_BIT) ||
        (host->quirks & SDHCI_QUIRK_BROKEN_HISPD_MODE)) {
        ctrl &= ~SDHCI_CTRL_HISPD;
        no_hispd_bit = true;
    }

    if (!no_hispd_bit) {
        if (mmc->selected_mode == MMC_HS ||
            mmc->selected_mode == SD_HS ||
            mmc->selected_mode == MMC_DDR_52 ||
            mmc->selected_mode == MMC_HS_200 ||
            mmc->selected_mode == MMC_HS_400 ||
            mmc->selected_mode == UHS_SDR25 ||
            mmc->selected_mode == UHS_SDR50 ||
            mmc->selected_mode == UHS_SDR104 ||
            mmc->selected_mode == UHS_DDR50)
            ctrl |= SDHCI_CTRL_HISPD;
        else
            ctrl &= ~SDHCI_CTRL_HISPD;
    }
    /* HOST_CONTROL
     * [0] 1: LED on, 0: LED off
     * [1] 1: 4-bit data transfer mode, 0: 1-bit
     * [2] 1: High Speed enable, 0: disable
     * [4-3] 0: SDMA, 2: ADMA2, 3: ADMA2/ADMA3
     * [5] 1: 8-bit extend data transfer mode, 0: [1]に従う
     * [6] 1: Card inserted, 0: no card
     * [7] 1:
     * [8] 1: Bus power on, 0: off
     * [11-9]: Bus voltage 111b: 3.3v, 110b: 3.0v, 101b: 1.8v
    */
    write8(SD0 + SDHCI_HOST_CONTROL, ctrl);
    delayms(5);
    debug("set host_contorl: 0x%08x", read8(SD0 + SDHCI_HOST_CONTROL));
    return 0;
}

// 指定の信号電圧をレジスタに書き込む
int sdhci_set_voltage(struct mmc *mmc)
{
    uint32_t ctrl;

    ctrl = read16(SD0 + SDHCI_HOST_CONTROL2);

    switch (mmc->signal_voltage) {
    case MMC_SIGNAL_VOLTAGE_330:
        ctrl &= ~SDHCI_CTRL_VDD_180;
        write16(SD0 + SDHCI_HOST_CONTROL2, ctrl);

        /* Wait for 5ms */
        delayms(5);

        /* 3.3V regulator output should be stable within 5 ms */
        ctrl = read16(SD0 + SDHCI_HOST_CONTROL2);
        if (ctrl & SDHCI_CTRL_VDD_180) {
            debug("3.3V regulator output did not become stable");
            return -1;
        }
        debug("switch to 3.3v");
        break;
    case MMC_SIGNAL_VOLTAGE_180:
        ctrl |= SDHCI_CTRL_VDD_180;
        write16(SD0 + SDHCI_HOST_CONTROL2, ctrl);
        // システムレベルで電圧を1.8vに切り替える
        cvi_sd_voltage_switch(mmc);

        /* 1.8V regulator output has to be stable within 5 ms */
        ctrl = read16(SD0 + SDHCI_HOST_CONTROL2);
        if (!(ctrl & SDHCI_CTRL_VDD_180)) {
            debug("1.8V regulator output did not become stable");
            return -1;
        }
        debug("switch to 1.8v");
        break;
    default:
        /* No signal voltage switch required */
        debug("no ndeed voltage swith")
        return -1;
    }
    // UHSスピードモードを設定する
    sdhci_set_uhs_timing(mmc);

    return 0;
}

/* 当面DMAは使わない
static void sdhci_prepare_dma(struct sdhci_host *host, struct mmc_data *data,
                  int *is_aligned, int trans_bytes)
{
    dma_addr_t dma_addr;
    unsigned char ctrl;
    void *buf;

    if (data->flags == MMC_DATA_READ)
        buf = data->dest;
    else
        buf = (void *)data->src;

    ctrl = read8(SD0 + SDHCI_HOST_CONTROL);
    ctrl &= ~SDHCI_CTRL_DMA_MASK;
    write8(SD0 + SDHCI_HOST_CONTROL, ctrl);

    if (host->flags & USE_SDMA &&
        (host->force_align_buffer ||
         (host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR &&
          ((unsigned long)buf & 0x7) != 0x0))) {
        *is_aligned = 0;
        if (data->flags != MMC_DATA_READ)
            memcpy(host->align_buffer, buf, trans_bytes);
        buf = host->align_buffer;
    }

    host->start_addr = dma_map_single(buf, trans_bytes,
                      mmc_get_dma_dir(data));

    if (host->flags & USE_SDMA) {
        dma_addr = dev_phys_to_bus(mmc_to_dev(host->mmc), host->start_addr);
        if (sdhci_readw(host, SDHCI_HOST_CONTROL2) & SDHCI_HOST_VER4_ENABLE) {
            sdhci_writel(host, dma_addr, SDHCI_ADMA_ADDRESS);
            sdhci_writel(host, (dma_addr >> 32), SDHCI_ADMA_ADDRESS_HI);
            sdhci_writel(host, data->blocks, SDHCI_DMA_ADDRESS);
            sdhci_writew(host, 0, SDHCI_BLOCK_COUNT);
        } else {
            sdhci_writel(host, dma_addr, SDHCI_DMA_ADDRESS);
            sdhci_writew(host, data->blocks, SDHCI_BLOCK_COUNT);
        }
    }
}
*/

static void sdhci_cmd_done(struct sdhci_host *host, struct mmc_cmd *cmd)
{
    int i;

    // R2を除いてresponseにはCRC7/end bitは含まない
    if (cmd->resp_type & MMC_RSP_136) {
        /* CRC is stripped so we need to do some shifting. */
        for (i = 0; i < 4; i++) {
            cmd->response[i] = read32(SD0 + SDHCI_RESPONSE + (3-i)*4) << 8;
            if (i != 3)
                cmd->response[i] |= read8(SD0 + SDHCI_RESPONSE + (3-i)*4-1);
        }
    } else {
        cmd->response[0] = read32(SD0 + SDHCI_RESPONSE);
    }
}

static void sdhci_transfer_pio(struct sdhci_host *host, struct mmc_data *data)
{
    int i;
    char *offs;
    for (i = 0; i < data->blocksize; i += 4) {
        offs = data->dest + i;
        if (data->flags == MMC_DATA_READ)
            *(uint32_t *)offs = read32(SD0 + SDHCI_BUFFER);
        else
            write32(SD0 + SDHCI_BUFFER, *(uint32_t *)offs);
    }
}

static int sdhci_transfer_data(struct sdhci_host *host, struct mmc_data *data)
{
    //dma_addr_t start_addr = host->start_addr;
    unsigned int stat, rdy, mask, timeout, block = 0;
    bool transfer_done = false;

    timeout = 1000000;
    rdy = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
    mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;
    do {
        stat = read32(SD0 + SDHCI_INT_STATUS);
        if (stat & SDHCI_INT_ERROR) {
            debug("%s: Error detected in status(0x%X)!",
                 __func__, stat);
            return -EIO;
        }
        if (!transfer_done && (stat & rdy)) {
            if (!(read32(SD0 + SDHCI_PRESENT_STATE) & mask))
                continue;
            write32(SD0 + SDHCI_INT_STATUS, rdy);
            sdhci_transfer_pio(host, data);
            data->dest += data->blocksize;
            if (++block >= data->blocks) {
                /* Keep looping until the SDHCI_INT_DATA_END is
                 * cleared, even if we finished sending all the
                 * blocks.
                 */
                transfer_done = true;
                continue;
            }
        }
    /*
        if ((host->flags & USE_DMA) && !transfer_done &&
            (stat & SDHCI_INT_DMA_END)) {
            write32(SD0 + SDHCI_INT_STATUS, SDHCI_INT_DMA_END);
            if (host->flags & USE_SDMA) {
                start_addr &=
                ~(SDHCI_DEFAULT_BOUNDARY_SIZE - 1);
                start_addr += SDHCI_DEFAULT_BOUNDARY_SIZE;
                start_addr =  start_addr;
                if (read16(SD0 + SDHCI_HOST_CONTROL2) & SDHCI_HOST_VER4_ENABLE) {
                    write32(SD0 + SDHCI_ADMA_ADDRESS, start_addr);
                    write32(SD0 + SDHCI_ADMA_ADDRESS_HI, (start_addr >> 32));
                } else {
                    write32(SD0 + SDHCI_DMA_ADDRESS, start_addr);
                }
            }
        }
    */

        if (timeout-- > 0)
            delayus(10);
        else {
            info("%s: Transfer data timeout", __func__);
            return -ETIMEDOUT;
        }
    } while (!(stat & SDHCI_INT_DATA_END));

/*
    dma_unmap_single(host->start_addr, data->blocks * data->blocksize,
             mmc_get_dma_dir(data));
*/
    return 0;
}

int sdhci_card_busy(struct mmc *mmc, int state, int timeout_us)
{
    int ret = -ETIMEDOUT;
    bool dat0_high;
    bool target_dat0_high = !!state;

    timeout_us = timeout_us / 10;
    if (timeout_us == 0)
        timeout_us = 1;
    while (timeout_us--) {
        dat0_high = !!(read32(SD0 + SDHCI_PRESENT_STATE) & BIT(20));
        if (dat0_high == target_dat0_high) {
            ret = 0;
            break;
        }
        delayus(10);
    }
    return ret;
}

int sdhci_send_command(struct mmc *mmc, struct mmc_cmd *cmd,
                  struct mmc_data *data)
{
    struct sdhci_host *host = mmc->priv;
    unsigned int stat = 0;
    int ret = 0;
    //int trans_bytes = 0, is_aligned = 1;
    uint32_t mask, flags, mode;
    unsigned int time = 0;
    uint64_t start = get_timer(0);

    host->start_addr = 0;
    /* Timeout unit - ms */
    static unsigned int cmd_timeout = SDHCI_CMD_DEFAULT_TIMEOUT;

    mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

    /* We shouldn't wait for data inihibit for stop commands, even
       though they might use busy signaling */
    if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION ||
        ((cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK ||
          cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) && !data))
        mask &= ~SDHCI_DATA_INHIBIT;

    // CMD/DATラインが空くのを待つ
    uint32_t state = read32(SD0 + SDHCI_PRESENT_STATE);
    while (state & mask) {
        debug("mask: 0x%08x, state: 0x%08x", mask, state);
        if (time >= cmd_timeout) {
            if (2 * cmd_timeout <= SDHCI_CMD_MAX_TIMEOUT) {
                cmd_timeout += cmd_timeout;
                debug("timeout increasing to: %u ms.",
                       cmd_timeout);
            } else {
                debug("timeout.");
                return -ECOMM;
            }
        }
        time++;
        delayus(1000);
        state = read32(SD0 + SDHCI_PRESENT_STATE);
    }

    // 割り込みフラグをクリア
    write32(SD0 + SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);

    mask = SDHCI_INT_RESPONSE;
    if ((cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK ||
         cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) && !data)
        mask = SDHCI_INT_DATA_AVAIL;

    if (!(cmd->resp_type & MMC_RSP_PRESENT))
        flags = SDHCI_CMD_RESP_NONE;
    else if (cmd->resp_type & MMC_RSP_136)
        flags = SDHCI_CMD_RESP_LONG;
    else if (cmd->resp_type & MMC_RSP_BUSY) {
        flags = SDHCI_CMD_RESP_SHORT_BUSY;
        mask |= SDHCI_INT_DATA_END;
    } else
        flags = SDHCI_CMD_RESP_SHORT;

    if (cmd->resp_type & MMC_RSP_CRC)
        flags |= SDHCI_CMD_CRC;
    if (cmd->resp_type & MMC_RSP_OPCODE)
        flags |= SDHCI_CMD_INDEX;
    if (data || cmd->cmdidx ==  MMC_CMD_SEND_TUNING_BLOCK ||
        cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200)
        flags |= SDHCI_CMD_DATA;

    /* データフラグに従い転送モードをセットする */
    if (data) {
        write8(SD0 + SDHCI_TIMEOUT_CONTROL, 0xe);
        mode = SDHCI_TRNS_BLK_CNT_EN;
        //trans_bytes = data->blocks * data->blocksize;
        if (data->blocks > 1)
            mode |= SDHCI_TRNS_MULTI;

        if (data->flags == MMC_DATA_READ)
            mode |= SDHCI_TRNS_READ;

/*
        if (host->flags & USE_DMA) {
            mode |= SDHCI_TRNS_DMA;
            sdhci_prepare_dma(host, data, &is_aligned, trans_bytes);
        }
*/
        write16(SD0 + SDHCI_BLOCK_SIZE, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
                data->blocksize));
        write16(SD0 + SDHCI_TRANSFER_MODE, mode);
    } else if (cmd->resp_type & MMC_RSP_BUSY) {
        write8(SD0 + SDHCI_TIMEOUT_CONTROL, 0xe);
    }
    // コマンド送信
    write32(SD0 + SDHCI_ARGUMENT, cmd->cmdarg);
    write16(SD0 + SDHCI_COMMAND, SDHCI_MAKE_CMD(cmd->cmdidx, flags));
    start = get_timer(0);
    uint64_t now;
    trace("mask: 0x%08x", mask);
    do {
        stat = read32(SD0 + SDHCI_INT_STATUS);
        if (stat & SDHCI_INT_ERROR)
            break;

        if ((now = get_timer(start)) >= SDHCI_READ_STATUS_TIMEOUT) {
            if (host->quirks & SDHCI_QUIRK_BROKEN_R1B) {
                return 0;
            } else {
                debug("Timeout for status update: stat: 0x%08x, now: 0x%016x", stat, now);
                return -ETIMEDOUT;
            }
        }
        trace("stat: 0x%08x, mask: 0x%08x, stat & mask: 0x%08x, (stat & mask) != mask: %s", stat, mask, stat & mask, (stat & mask) != mask ? "true" : "false");
    } while ((stat & mask) != mask);

    if ((stat & (SDHCI_INT_ERROR | mask)) == mask) {
        // レスポンスの調整
        sdhci_cmd_done(host, cmd);
        write32(SD0 + SDHCI_INT_STATUS, mask);
    } else
        ret = -1;

    // データの送信
    if (!ret && data) {
        ret = sdhci_transfer_data(host, data);
    }

    if (host->quirks & SDHCI_QUIRK_WAIT_SEND_CMD)
        delayus(1000);

    // 割り込み状態を取得してクリア
    stat = read32(SD0 + SDHCI_INT_STATUS);
    write32(SD0 + SDHCI_INT_STATUS, SDHCI_INT_ALL_MASK);


    if (!ret) {
    /*
        if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) &&
                !is_aligned && (data->flags == MMC_DATA_READ))
            memcpy(data->dest, host->align_buffer, trans_bytes);
    */
        return 0;
    }

    // CMD/DATライノのリセット
    sdhci_reset(host, SDHCI_RESET_CMD);
    sdhci_reset(host, SDHCI_RESET_DATA);
    if (stat & SDHCI_INT_TIMEOUT) {
        debug("SDHCI_INT_TIMEOUT");
        return -ETIMEDOUT;
    } else {
        debug("ECOMM")
        return -ECOMM;
    }
}

// sdhciの初期化
int sdhci_init(struct mmc *mmc)
{
    // FIXME: sdhci_hostの割当をどこでするのが良いか検討
    struct sdhci_host *host = &sdhci_host;

    // 1. devicetreeで定義されている定数を設定する
    host->name = SDHCI_NAME;
    host->ioaddr = (void *)SDHCI_IOADDR;  // SD0の基底アドレス
    host->bus_width = SDHCI_BUS_WIDTH;
    host->max_clk = SDHCI_MAX_CLK;
    if (SDHCI_NO_1_8_V)                   // 1.8vは使わない
        host->quirks |= SDHCI_QUIRK_NO_1_8_V;

    int ret = sdhci_setup_cfg(&host->cfg, host, SDHCI_MMC_FMAX_FREQ, SDHCI_MMC_FMIN_FREQ);
    if (ret) {
        error("sdhci_init: failed sdhci_setup_cfg: %n", ret);
        return ret;
    }

    // 2. 定数以外のsdhci_hostの初期化を行うする
    sdhci_host_init(host);

    // 3. mmcにsdhci_hostへのポインタをセット
    mmc->priv = host;
    mmc->cfg = &host->cfg;

    info("sdhci_init ok");

    return 0;
}
