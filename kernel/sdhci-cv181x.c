/*
 * Copyright (C) 2016 Socionext Inc.
 *   Author: Masahiro Yamada <yamada.masahiro@socionext.com>
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <mmc.h>
#include <sdhci.h>
#include <riscv-mmio.h>
#include <printf.h>
#include <errno.h>

#define MMC_TYPE_MMC  0       /* MMC card */
#define MMC_TYPE_SD   1       /* SD card */
#define MMC_TYPE_SDIO 2       /* SDIO card */

// cviのプライベートデータ構造体    : cv180xの情報
struct cvi_sdhci_host {
    struct sdhci_host *host;
    int pll_index;
    int pll_reg;
    int no_1_8_v;
    int is_64_addressing;
    int reset_tx_rx_phy;
    uint32_t mmc_fmax_freq;
    uint32_t mmc_fmin_freq;
};

static struct sdhci_host sdhci_host;
static struct cvi_sdhci_host cvi_sdhci_host;

// PAD SD0_XXX (0x300_1XXX)のPD/PUの設定
static void cvi_sdio0_pad_setting(bool reset)
{
    if (reset) {
        mmio_clrsetbits_32(REG_SDIO0_PWR_EN_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_PWR_EN_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_CD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CD_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_CLK_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CLK_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_CMD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CMD_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT1_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT1_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT0_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT0_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT2_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT2_PAD_RESET << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT3_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT3_PAD_RESET << REG_SDIO0_PAD_SHIFT);

    } else {
        mmio_clrsetbits_32(REG_SDIO0_PWR_EN_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_PWR_EN_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_CD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CD_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_CLK_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CLK_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_CMD_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_CMD_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT1_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT1_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT0_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT0_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT2_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT2_PAD_VALUE << REG_SDIO0_PAD_SHIFT);

        mmio_clrsetbits_32(REG_SDIO0_DAT3_PAD_REG, REG_SDIO0_PAD_CLR_MASK,
                   REG_SDIO0_DAT3_PAD_VALUE << REG_SDIO0_PAD_SHIFT);
    }
}

// PAD SD0_XXX の機能選択
static void cvi_sdio0_pad_function(bool bunplug)
{
    /* bunplugがfalseの場合は、SDIO0として使用、
     *          trueの場合は、gpioとして使用
     * Pinout-v1: 2. SD0機能選択を参照 (file:///Volumes/wdb/xv6-milkv/doc/book/cv180x/pinout/table_1.html)
     * Name                unplug plug
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

    mmio_write_32(PAD_SDIO0_CD_REG, 0x0);
    mmio_write_32(PAD_SDIO0_PWR_EN_REG, 0x0);
    mmio_write_32(PAD_SDIO0_CLK_REG, val);
    mmio_write_32(PAD_SDIO0_CMD_REG, val);
    mmio_write_32(PAD_SDIO0_D0_REG, val);
    mmio_write_32(PAD_SDIO0_D1_REG, val);
    mmio_write_32(PAD_SDIO0_D2_REG, val);
    mmio_write_32(PAD_SDIO0_D3_REG, val);
}

// dtsからcvi固有データを抽出
static int cvi_ofdata_to_platdata(struct cvi_sdhci_host *cvi_host)
{
    struct sdhci_host *host = cvi_host->host;

    host->name = "cvi_sdhci";
    host->ioaddr = (void *)0x4310000;   // reg
    host->bus_width = 4;                // width
    host->max_clk = 375000000;          // src-frequency

    cvi_host->mmc_fmin_freq = 400000;       // min-frequency
    cvi_host->mmc_fmax_freq = 200000000;    // max-frequency
    cvi_host->is_64_addressing = true;      // 64_addressing
    cvi_host->reset_tx_rx_phy = true;       // reset_tx_rx_phy
    cvi_host->no_1_8_v = NO_1_8_V;          // no-1-8-v
    cvi_host->pll_index = 6;                // pll_index
    cvi_host->pll_reg = 0x3002070;          // pll_reg

    if (cvi_host->no_1_8_v) {
        host->quirks |= SDHCI_QUIRK_NO_1_8_V;
        // no-1-8-v = true の場合
        host->mmc->cfg->host_caps &= ~(UHS_CAPS | MMC_MODE_HS200 |
                    MMC_MODE_HS400 | MMC_MODE_HS400_ES);
    }

    return 0;
}

// cv181x固有のレジスタの初期設定
static void cvi_mmc_set_tap(struct sdhci_host *host, uint16_t tap)
{
    trace("tap: %d", tap);
    // SDクロックを無効にする : sd_clk_en(0x2c[2]) = 0
    sdhci_writew(host, sdhci_readw(host, SDHCI_CLOCK_CONTROL) & (~(BIT(2))), SDHCI_CLOCK_CONTROL);
    // MSHC_CTRL を全クリア
    sdhci_writel(host, sdhci_readl(host, CVI_SDHCI_VENDOR_MSHC_CTRL_R) & (~(BIT(1))),
        CVI_SDHCI_VENDOR_MSHC_CTRL_R);
    // PHY_TX_RX_DLYをセット
    sdhci_writel(host, BIT(8) | (tap << 16), CVI_SDHCI_PHY_TX_RX_DLY);
    // PHY_CONFIG = 0 (PHY tx data path = pipe enalbe)
    sdhci_writel(host, 0, CVI_SDHCI_PHY_CONFIG);
    // SDクロックを有効にする : sd_clk_en(0x2c[2]) = 1
    sdhci_writew(host, sdhci_readw(host, SDHCI_CLOCK_CONTROL) | BIT(2), SDHCI_CLOCK_CONTROL);
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

// チューニングの後始末をする
static void reset_after_tuning_pass(struct sdhci_host *host)
{
    //debug("start");
    //printf("1");
    /* BUF_RD_READY 割り込みをクリア */
    sdhci_writew(host, sdhci_readw(host, SDHCI_INT_STATUS) & (~(0x1 << 5)),
             SDHCI_INT_STATUS);
    //printf("2");
    /* DATAラインをクリア = バッファにあるチューニングブロックをクリア :
     *   SDHCI_SOFTWARE_RESET.SW_RST_DAT = 1 */
    sdhci_writeb(host, sdhci_readb(host, SDHCI_SOFTWARE_RESET) | (0x1 << 2), SDHCI_SOFTWARE_RESET);
    //printf("3");
    /* コマンドラインをクリア : SDHCI_SOFTWARE_RESET.SW_RST_CMD = 1    */
    sdhci_writeb(host, sdhci_readb(host, SDHCI_SOFTWARE_RESET) | (0x1 << 1), SDHCI_SOFTWARE_RESET);
    //printf("4");
    // CMD/DATAラインがクリアされるまで待機
    while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & 0x3)
        ;
    //printf("5\n");
}

// cv181xのチューニング実行関数
int cvi_general_execute_tuning(struct mmc *mmc, uint8_t opcode)
{
    trace("opcode: %d", opcode);
    uint16_t min = 0;
    uint32_t k = 0;
    int32_t ret;
    uint32_t retry_cnt = 0;

    uint32_t tuning_result[4] = {0, 0, 0, 0};
    uint32_t rx_lead_lag_result[4] = {0, 0, 0, 0};
    char tuning_graph[TUNE_MAX_PHCODE + 1];
    char rx_lead_lag_graph[TUNE_MAX_PHCODE + 1];

    //uint16_t reg = 0;
    uint16_t reg_rx_lead_lag = 0;
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

    uint16_t ctl2; // norm_stat_en_b, err_stat_en_b,
    uint32_t stat_en_b, norm_signal_en_b;

    // 割り込み関係のレジスタの現在値をバックアップ
    //norm_stat_en_b = sdhci_readw(host, SDHCI_INT_ENABLE);
    //err_stat_en_b = sdhci_readw(host, SDHCI_ERR_INT_STATUS_EN);
    stat_en_b = sdhci_readl(host, SDHCI_INT_ENABLE);
    norm_signal_en_b = sdhci_readl(host, SDHCI_SIGNAL_ENABLE);

    //reg = sdhci_readw(host, SDHCI_ERR_INT_STATUS);
    trace("mmc%d : SDHCI_ERR_INT_STATUS 0x%x", host->index, reg);

    //reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
    uint32_t reg_32 = sdhci_readl(host, SDHCI_ACMD12_ERR);
    trace("3c = 0x%08x, 3e = 0x%04x", reg_32, reg);
    //debug("mmc%d : host ctrl2 (B) 0x%x", host->index, reg);
    /* Host_CTRL2_R.SAMPLE_CLK_SEL=0 */
    // B[7] = 0 :データのサンプルに固定クロックを使用
    reg_32 &= ~(0x1 << 23);
    reg_32 &= ~(0x3 << 20);
    trace("reg_32 = 0x%04x", reg_32);
    //sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
    sdhci_writel(host, reg_32, SDHCI_ACMD12_ERR);
    //sdhci_writew(host, sdhci_readw(host, SDHCI_HOST_CONTROL2) & ~(0x1 << 7),
     //        SDHCI_HOST_CONTROL2);
    // B[4:5] = 0 :Driver Type Bを選択
    //sdhci_writew(host, sdhci_readw(host, SDHCI_HOST_CONTROL2) & ~(0x3 << 4),
    //         SDHCI_HOST_CONTROL2);

    //reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
    //debug("mmc%d : host ctrl2 (A) 0x%x", host->index, reg);
    //reg_32 = sdhci_readl(host, SDHCI_ACMD12_ERR);
    trace("3c = 0x%08x, 3e = 0x%04x", reg_32, reg);

    while (min < TUNE_MAX_PHCODE) {
        //printf(">");
        retry_cnt = 0;
        // MSHC_CTRL.CLK_FREE_EN = 1
        //sdhci_writew(host, BIT(2), CVI_SDHCI_VENDOR_OFFSET);
        cvi_mmc_set_tap(host, min);
        reg_rx_lead_lag = sdhci_readw(host, CVI_SDHCI_PHY_DLY_STS) & BIT(1);
        trace("reg_rx_lead_lag: %d", reg_rx_lead_lag);

retry_tuning:
        // チューニングコマンドを送信 : MAX_TUNING_CMD_RETRY_COUNT回実行
        //printf(".");
        ret = mmc_send_tuning(mmc, opcode, NULL);
        if (ret == 0 && retry_cnt < MAX_TUNING_CMD_RETRY_COUNT) {
            retry_cnt++;
            goto retry_tuning;
        }
        //printf("|");
        trace("retry_cnt: %d", retry_cnt);

        if (ret) {
            SET_MASK_BIT(tuning_result, min);
        }

        if (reg_rx_lead_lag) {
            SET_MASK_BIT(rx_lead_lag_result, min);
        }

        min++;
    }
    //printf("\n");
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
    ret = mmc_send_tuning(host->mmc, opcode, NULL);
    info("mmc%d : finished tuning, code:%d", host->index, final_tap);

    ctl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
    // HOST_CONTROL2.EXECUTE = 0
    ctl2 &= ~SDHCI_CTRL_EXEC_TUNING;
    sdhci_writew(host, ctl2, SDHCI_HOST_CONTROL2);
    // 割り込み系レジスタを復元する
    //sdhci_writew(host, norm_stat_en_b, SDHCI_INT_ENABLE);
    sdhci_writel(host, stat_en_b, SDHCI_INT_ENABLE);
    sdhci_writel(host, norm_signal_en_b, SDHCI_SIGNAL_ENABLE);
    //sdhci_writew(host, err_stat_en_b, SDHCI_ERR_INT_STATUS_EN);


    return ret;
}

// cvi固有のリセット処理
void cvi_general_reset(struct sdhci_host *host, uint8_t mask)
{
    uint16_t ctrl_2;
    // 0x3E
    ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
    //debug("MMC%d : ctrl_2 = 0x%04x", host->index, ctrl_2);

    // UHSモードの選択
    ctrl_2 &= SDHCI_CTRL_UHS_MASK;
    if (ctrl_2 == SDHCI_CTRL_UHS_SDR104) {
        //reg_0x200[1] = 0
        sdhci_writel(host,
                sdhci_readl(host, CVI_SDHCI_VENDOR_MSHC_CTRL_R) & ~(BIT(1)),
                CVI_SDHCI_VENDOR_MSHC_CTRL_R);
        //reg_0x200[16] = 1 for sd1
        if (host->index == MMC_TYPE_SDIO) {
            sdhci_writel(host,
                    sdhci_readl(host, CVI_SDHCI_VENDOR_MSHC_CTRL_R) | BIT(16),
                    CVI_SDHCI_VENDOR_MSHC_CTRL_R);
        }
        //reg_0x24c[0] = 0 : pipe enable
        sdhci_writel(host,
                 sdhci_readl(host, CVI_SDHCI_PHY_CONFIG) & ~(BIT(0)),
                 CVI_SDHCI_PHY_CONFIG);
        //reg_0x240[22:16] = tap reg_0x240[9:8] = 1 reg_0x240[6:0] = 0
        sdhci_writel(host, (BIT(8) | ((0 & 0x7F) << 16)), CVI_SDHCI_PHY_TX_RX_DLY);
    } else {
        // DS/HSの設定でリセット
        //reg_0x200[1] = 1
        sdhci_writel(host,
                sdhci_readl(host, CVI_SDHCI_VENDOR_MSHC_CTRL_R) | BIT(1),
                CVI_SDHCI_VENDOR_MSHC_CTRL_R);
        //reg_0x200[16] = 1 for sd1
        if (host->index == MMC_TYPE_SDIO) {
            sdhci_writel(host,
                    sdhci_readl(host, CVI_SDHCI_VENDOR_MSHC_CTRL_R) | BIT(16),
                    CVI_SDHCI_VENDOR_MSHC_CTRL_R);
        }
        //reg_0x24c[0] = 1
        sdhci_writel(host,
                sdhci_readl(host, CVI_SDHCI_PHY_CONFIG) | BIT(0),
                CVI_SDHCI_PHY_CONFIG);
        //reg_0x240[25:24] = 1 reg_0x240[22:16] = 0 reg_0x240[9:8] = 1 reg_0x240[6:0] = 0
        sdhci_writel(host, 0x1000100, CVI_SDHCI_PHY_TX_RX_DLY);
    }

    trace("CVI_SDHCI_PHY_CONFIG = 0x%08x, CVI_SDHCI_VENDOR_OFFSET = 0x%08x",
         sdhci_readl(host, CVI_SDHCI_PHY_CONFIG),
         sdhci_readl(host, CVI_SDHCI_VENDOR_OFFSET));
    trace("CVI_SDHCI_PHY_TX_RX_DLY = 0x%08x, CVI_SDHCI_PHY_DLY_STS = 0x%08x",
         sdhci_readl(host, CVI_SDHCI_PHY_TX_RX_DLY),
         sdhci_readl(host, CVI_SDHCI_PHY_DLY_STS));
}

// 信号電圧を切替える
void cvi_sd_voltage_switch(struct mmc *mmc)
{
    //enable SDIO0_CLK[7:5] to set CLK max strengh
    mmio_setbits_32(REG_SDIO0_CLK_PAD_REG, BIT(7) | BIT(6) | BIT(5));

    //Voltage switching flow (1.8v)
    //reg_pwrsw_auto=1, reg_pwrsw_disc=0, pwrsw_vsel=1(1.8v), reg_en_pwrsw=1
    mmio_write_32(SYSCTL_BASE + REG_TOP_SD_PWRSW_CTRL, 0xB);

    //wait 1ms
    delayms(1);
}

// カードの有無をチェックする: 挿入されていれば 1
int cvi_get_cd(struct sdhci_host *host)
{
    uint32_t reg;

    reg = sdhci_readl(host, SDHCI_PRESENT_STATE);
    trace("SDHCI_PRESENT_STATE = 0x%08x", reg);
    if (reg & SDHCI_CARD_PRESENT) {
        return 1;
    } else {
        return 0;
    }
}

// cvi sdhciのプローブ関数
int cvi_sdhci_probe(struct mmc *mmc)
{
    struct cvi_sdhci_host *cvi_host = &cvi_sdhci_host;
    struct sdhci_host *host = &sdhci_host;
    int ret;

    host->mmc = mmc;
    host->mmc->priv = host;
    host->index = MMC_TYPE_SD;
    cvi_host->host = host;

    // sdhci_cv180xのセットアップ
    ret = cvi_ofdata_to_platdata(cvi_host);
    if (ret)
        return ret;

    host->host_caps = mmc->cfg->host_caps;

    // sdhciのセットアップ
    ret = sdhci_setup_cfg(mmc->cfg, host, cvi_host->mmc_fmax_freq, cvi_host->mmc_fmin_freq);
    if (ret)
        return ret;

    if (host->index == MMC_TYPE_SD) {
        // カードが挿入されているか判断できる場合
        int present = cvi_get_cd(host);
        // カードが挿入されている
        if (present == 1) {
            // 電圧を3.3Vにしてパワーオン : Voltage switching flow (3.3)
            //(b[3]:reg_pwrsw_auto=1, b[2]:reg_pwrsw_disc=0, b[1]:reg_pwrsw_vsel=0(3.0v),
            // b[0]:reg_en_pwrsw=1)
            // sd_pwrsw_ctrlレジスタ : 0x030001F4
            mmio_write_32(SYSCTL_BASE + REG_TOP_SD_PWRSW_CTRL, 0x9);
            // 必要な関係PADをPull UP
            cvi_sdio0_pad_function(false);
            cvi_sdio0_pad_setting(false);
        // 挿入されていない
        } else {
            // 電圧を止める : Voltage close flow
            //(reg_pwrsw_auto=1, reg_pwrsw_disc=1, reg_pwrsw_vsel=1(1.8v), reg_en_pwrsw=0)
            mmio_write_32(SYSCTL_BASE + REG_TOP_SD_PWRSW_CTRL, 0xE);
            // かんけいPADをPull Down
            cvi_sdio0_pad_function(true);
            cvi_sdio0_pad_setting(true);
        }
    } else {
        info("wrong host index : %d !!", host->index);
        return -ENXIO;
    }

    // sdchiのプローブ処理を行う(sdhciの初期化を行う)
    ret = sdhci_probe(mmc);

    // 該当するが、SDHost 3.0ではこのフィールドは未定義
    if (cvi_host->is_64_addressing) {
        sdhci_writew(host, sdhci_readw(host, SDHCI_HOST_CONTROL2)
                | SDHCI_HOST_VER4_ENABLE | SDHCI_HOST_ADDRESSING,
                SDHCI_HOST_CONTROL2);
    }

    // 該当: 0x200, 0x240, 0x24cをセット
    if (cvi_host->reset_tx_rx_phy) {
        /* Default value */
        sdhci_writel(host, sdhci_readl(host, CVI_SDHCI_VENDOR_MSHC_CTRL_R) | BIT(1) | BIT(8) | BIT(9),
            CVI_SDHCI_VENDOR_MSHC_CTRL_R);
        sdhci_writel(host, 0x01000100, CVI_SDHCI_PHY_TX_RX_DLY);
        sdhci_writel(host, 00000001, SDHCI_PHY_CONFIG);
    }

    return ret;
}
