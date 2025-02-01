// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2011, Marvell Semiconductor Inc.
 * Lei Wen <leiwen@marvell.com>
 *
 * Back ported to the 8xx platform (from the 8260 platform) by
 * Murray.Jensen@cmst.csiro.au, 27-Jan-01.
 */

#include <defs.h>
#include <mmc.h>
#include <sdhci.h>
#include <dma-mapping.h>
#include <common/types.h>
#include <printf.h>
#include <memalign.h>
#include <errno.h>
#include <bitops.h>

// ラインをリセットする
static void sdhci_reset(struct sdhci_host *host, uint8_t mask)
{
    unsigned long timeout;

    /* Wait max 100 ms */
    timeout = 100;
    // ラインのソフトウェアリセットを行う
    sdhci_writeb(host, mask, SDHCI_SOFTWARE_RESET);
    // ラインのソフトウェアリセット処理が終わるのを待つ
    while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) {
        if (timeout == 0) {
            warn("Reset 0x%x never completed.", (int)mask);
            return;
        }
        timeout--;
        delayus(1000);
    }
    // cv180x固有のリセット処理を実行する
    cvi_general_reset(host, mask);
}

// コマンド終了処理: レスポンス情報を取得する
static void sdhci_cmd_done(struct sdhci_host *host, struct mmc_cmd *cmd)
{
    int i;
    // レスポンスがR2の場合
    if (cmd->resp_type & MMC_RSP_136) {
        /* CRC(8bit)が剥がされているのでその分シフトする必要がある */
        for (i = 0; i < 4; i++) {
            // response[0] = RES127_96 << 8 | RESP95_64[31:24]
            // response[1] = RESP95_64 << 8 | RESP63_32[31:24]
            // response[0] = RESP63_32 << 8 | RESP31-0[31:24]
            // response[0] = RESP31 << 8
            cmd->response[i] = sdhci_readl(host,
                    SDHCI_RESPONSE + (3-i)*4) << 8;
            if (i != 3)
                cmd->response[i] |= sdhci_readb(host,
                        SDHCI_RESPONSE + (3-i)*4-1);
        }
    // それ以外のレスポンス
    } else {
        cmd->response[0] = sdhci_readl(host, SDHCI_RESPONSE);
    }
}

// BUF_DATAレジスタ(020)を使って実際にデータの送受信を行う
static void sdhci_transfer_pio(struct sdhci_host *host, struct mmc_data *data)
{
    int i;
    char *offs;
    trace("start: size: %d, dst: %p", data->blocksize, data->dest);
    for (i = 0; i < data->blocksize; i += 4) {
        offs = data->dest + i;
        if (data->flags == MMC_DATA_READ)
            *(uint32_t *)offs = sdhci_readl(host, SDHCI_BUFFER);
        else
            sdhci_writel(host, *(uint32_t *)offs, SDHCI_BUFFER);
    }
}

// DMAによる送信の準備を行う
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

    // DMAのタイプをセットする
    ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
    // a. SDMAとする (HOST_CONtROL.B[4:3] = 0x0)
    ctrl &= ~SDHCI_CTRL_DMA_MASK;
    // b. ADMA３とする（該当せず）
    if (host->flags & USE_ADMA64)
        ctrl |= SDHCI_CTRL_ADMA64;
    // c. ADMA2とする（該当せず）
    else if (host->flags & USE_ADMA)
        ctrl |= SDHCI_CTRL_ADMA32;
    sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

    // 前半は該当、後半は該当せずで該当せず
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
    trace("start_addr: 0x%x", host->start_addr);
    if (host->flags & USE_SDMA) {
        dma_addr = host->start_addr;
        if (sdhci_readw(host, SDHCI_HOST_CONTROL2) & SDHCI_HOST_VER4_ENABLE) {
            sdhci_writel(host, dma_addr, SDHCI_ADMA_ADDRESS);
            sdhci_writel(host, (dma_addr >> 32), SDHCI_ADMA_ADDRESS_HI);
            sdhci_writel(host, data->blocks, SDHCI_DMA_ADDRESS);
            sdhci_writew(host, 0, SDHCI_BLOCK_COUNT);
        } else {
            sdhci_writel(host, dma_addr, SDHCI_DMA_ADDRESS);
            sdhci_writew(host, data->blocks, SDHCI_BLOCK_COUNT);
            trace("SDMA: addr: 0x%x, block_count: %d",
                sdhci_readl(host, SDHCI_DMA_ADDRESS), sdhci_readl(host, SDHCI_BLOCK_COUNT));
        }
    }
}

// データを転送
static int sdhci_transfer_data(struct sdhci_host *host, struct mmc_data *data)
{
    dma_addr_t start_addr = host->start_addr;
    unsigned int stat, rdy, mask, timeout, block = 0;
    bool transfer_done = false;
    trace("start: block: %d", data->blocks);

    timeout = 1000000;
    // NORM_AND_ERR_INT_STS.B[4]: Buffer Write Ready, B[5]: Buffer Read Ready
    rdy = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
    // PRESENT_STS.B[11]: Buffer Read Enable, B[10]: BUffer Write Enable
    mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;
    do {
        // NORM_AND_ERR_INT_STS(0x30)
        stat = sdhci_readl(host, SDHCI_INT_STATUS);
        // B[15]: ERR_INT エラーあり
        if (stat & SDHCI_INT_ERROR) {
            error("Error detected in status(0x%x)!", stat);
            return -EIO;
        }
        trace("done: %s, stat: 0x%08x, flags: 0x%08x", transfer_done ? "t" : "f", stat, host->flags);
        // 転送未完でバッファは準備OK
        if (!transfer_done && (stat & rdy)) {
            // PRESENT_STS(0x24) Read/WriteがEnableでない
            if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) & mask))
                continue;
            // 割り込みをクリア
            sdhci_writel(host, rdy, SDHCI_INT_STATUS);
            // 実際にデータの送受信を行う
            trace("pio: block: %d", block);
            sdhci_transfer_pio(host, data);
            data->dest += data->blocksize;
            if (++block >= data->blocks) {
                /* すべてのブロックを送信し終えても
                 * SDHCI_INT_DATA_END がクリアされるまで
                 * ループを続ける
                 */
                transfer_done = true;
                continue;
            }
        }
        // DMAがSDMA_BUF_BOUNDARYに達した (4K)ので次のstart_addrを設定して実行する
        // 4K未満のDMA転送の場合は関係しない
        if ((host->flags & USE_DMA) && !transfer_done &&
            (stat & SDHCI_INT_DMA_END)) {
            // 割り込みをクリア
            sdhci_writel(host, SDHCI_INT_DMA_END, SDHCI_INT_STATUS);
            trace("dma");
            if (host->flags & USE_SDMA) {
                // start_addrを512KB境界に合わせる
                start_addr &= ~(SDHCI_DEFAULT_BOUNDARY_SIZE - 1);
                start_addr += SDHCI_DEFAULT_BOUNDARY_SIZE;
                trace("start_addr: 0x%x", start_addr);
                // start_addrをバスアドレスに変換
                // start_addr = dev_phys_to_bus(mmc_to_dev(host->mmc), start_addr) = star_addr;
                // ADMA
                if (sdhci_readw(host, SDHCI_HOST_CONTROL2) & SDHCI_HOST_VER4_ENABLE) {
                    sdhci_writel(host, start_addr, SDHCI_ADMA_ADDRESS);
                    sdhci_writel(host, (start_addr >> 32), SDHCI_ADMA_ADDRESS_HI);
                // SMDA
                } else {
                    sdhci_writel(host, start_addr, SDHCI_DMA_ADDRESS);
                }
            }
        }
        if (timeout-- > 0)
            delayus(10);
        else {
            warn("Transfer data timeout");
            return -ETIMEDOUT;
        }
    // 転送完了までループ
    } while (!(stat & SDHCI_INT_DATA_END));

    // DMA転送の後始末
    if (host->flags & USE_DMA) {
        trace("unmap: addr: 0x%x, len: %d, dir: %d",
            host->start_addr, data->blocks * data->blocksize, mmc_get_dma_dir(data));
        dma_unmap_single(host->start_addr, data->blocks * data->blocksize,
                mmc_get_dma_dir(data));
    }

    return 0;
}
/*
 * タイムアウト定数
 *
 * カードがビジーの場合、ドライバはコマンドを送信しない。
 * そのため、ドライバはカードがレディになるのを待つ必要がある。
 * タイムアウトした際にカードがビジーであった場合、グローバルに
 * 定義された最大値を超えなければ(最後の)タイムアウト値を2倍に
 * する。各関数呼び出しは最後のタイムアウト値を使用する。
 */
#define SDHCI_CMD_MAX_TIMEOUT            3200
#define SDHCI_CMD_DEFAULT_TIMEOUT        100
#define SDHCI_READ_STATUS_TIMEOUT        1000

// 実際にコマンドを送信する関数 : dataが指定されていればdataを送受信する
int sdhci_send_command(struct mmc *mmc, struct mmc_cmd *cmd,
                  struct mmc_data *data)
{
    struct sdhci_host *host = mmc->priv;
    unsigned int stat = 0;
    int ret = 0;
    int trans_bytes = 0, is_aligned = 1;
    uint32_t mask, flags, mode = 0;
    unsigned int time = 0;
    unsigned long start = get_timer(0);

    host->start_addr = 0;
    /* Timeout unit - ms */
    static unsigned int cmd_timeout = SDHCI_CMD_DEFAULT_TIMEOUT;

    mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

    /* stopコマンドの場合は、たとえビジー信号が使われていても、
     * data inihibitを待つべきではない */
    if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION ||
        ((cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK ||
          cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) && !data))
        mask &= ~SDHCI_DATA_INHIBIT;
    //printf(" mask1: 0x%08x ", mask);
    // CMDラインが開くのを待つ
    while (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) {
        if (time >= cmd_timeout) {
            trace("MMC: mmc busy ");
            if (2 * cmd_timeout <= SDHCI_CMD_MAX_TIMEOUT) {
                cmd_timeout += cmd_timeout;
                warn("timeout increasing to: %u ms.", cmd_timeout);
            } else {
                error("timeout.");
                return -ECOMM;
            }
        }
        time++;
        delayus(1000);
    }

    // 割り込み状態をすべてクリア
    sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);

    mask = SDHCI_INT_RESPONSE;
    if ((cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK ||
         cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) && !data)
        mask = SDHCI_INT_DATA_AVAIL;
    // レスポンス種別をflagsにセット : XFER_MODE_AND_CMD.B[16:17]
    if (!(cmd->resp_type & MMC_RSP_PRESENT))
        flags = SDHCI_CMD_RESP_NONE;
    else if (cmd->resp_type & MMC_RSP_136)
        flags = SDHCI_CMD_RESP_LONG;
    else if (cmd->resp_type & MMC_RSP_BUSY) {
        flags = SDHCI_CMD_RESP_SHORT_BUSY;
        mask |= SDHCI_INT_DATA_END;
    } else
        flags = SDHCI_CMD_RESP_SHORT;
    // XFER_MODE_AND_CMD.B[19]
    if (cmd->resp_type & MMC_RSP_CRC)
        flags |= SDHCI_CMD_CRC;
    // XFER_MODE_AND_CMD.B[20]
    if (cmd->resp_type & MMC_RSP_OPCODE)
        flags |= SDHCI_CMD_INDEX;
    // XFER_MODE_AND_CMD.B[21]
    if (data || cmd->cmdidx ==  MMC_CMD_SEND_TUNING_BLOCK ||
        cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200)
        flags |= SDHCI_CMD_DATA;

    trace("mask: 0x%08x, flags: 0x%08x ", mask, flags);

    /* dataのあるなしに基づいて転送モードをセットする */
    // 1. データあり
    if (data) {
        // データタイムアウトをセット
        sdhci_writeb(host, 0xe, SDHCI_TIMEOUT_CONTROL);
        // XFER_MODE_AND_CMD.B[1]
        mode = SDHCI_TRNS_BLK_CNT_EN;
        // 転送データバイト数
        trans_bytes = data->blocks * data->blocksize;
        // マルチブロックか: XFER_MODE_AND_CMD.B[5]
        if (data->blocks > 1)
            mode |= SDHCI_TRNS_MULTI;
        // 転送の方向: XFER_MODE_AND_CMD.B[4]
        if (data->flags == MMC_DATA_READ)
            mode |= SDHCI_TRNS_READ;
        // DMAを使うか: XFER_MODE_AND_CMD.B[0]
        if (host->flags & USE_DMA) {
            mode |= SDHCI_TRNS_DMA;
            trace("use DMA: byte: %d", trans_bytes);
            sdhci_prepare_dma(host, data, &is_aligned, trans_bytes);
        }
        // BLK_SIZE_AND_CNTレジスタに書き込む: 512KB境界
        sdhci_writew(host, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
                data->blocksize),
                SDHCI_BLOCK_SIZE);
        // XFER_MODEレジスタ(0xc)に書き込む
        sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
    // 2. データなし
    } else if (cmd->resp_type & MMC_RSP_BUSY) {
        // タイムアウトをセット
        sdhci_writeb(host, 0xe, SDHCI_TIMEOUT_CONTROL);
    }

    trace("cmd: idx: %d, arg: 0x%x, rtype: %d, mask: 0x%x, flags: 0x%x, mode: 0x%x",
        cmd->cmdidx, cmd->cmdarg, cmd->resp_type, mask, flags, mode);

    // ARGUMENTレジスタに書き込む
    sdhci_writel(host, cmd->cmdarg, SDHCI_ARGUMENT);
    // CMDレジスタ(0xe)に書き込む: 転送が開始される
    sdhci_writew(host, SDHCI_MAKE_CMD(cmd->cmdidx, flags), SDHCI_COMMAND);
    start = get_timer(0);
    do {
        // 割り込みステータスを読み込む: NORM_AND_INT_STATUSレジスタ
        stat = sdhci_readl(host, SDHCI_INT_STATUS);
        // エラーあり: B[15]: INT_ERR = 1
        if (stat & SDHCI_INT_ERROR)
            break;
        // タイムアウト判定
        if (get_timer(start) >= SDHCI_READ_STATUS_TIMEOUT) {
            if (host->quirks & SDHCI_QUIRK_BROKEN_R1B) {
                return 0;
            } else {
                error("Timeout for status update!");
                return -ETIMEDOUT;
            }
        }
    } while ((stat & mask) != mask);    // mask = CMD_CMPL | XFER_CMPL

    // コマンド成功
    if ((stat & (SDHCI_INT_ERROR | mask)) == mask) {
        // レスポンスを取得
        sdhci_cmd_done(host, cmd);
        // 割り込み状態をクリア
        sdhci_writel(host, mask, SDHCI_INT_STATUS);
    } else {
        ret = -1;
    }

    // コマンド成功でデータがある場合はデータを転送
    if (!ret && data) {
        ret = sdhci_transfer_data(host, data);
    }

    if (host->quirks & SDHCI_QUIRK_WAIT_SEND_CMD)
        delayus(1000);

    // 割り込みステータスを読み込む
    stat = sdhci_readl(host, SDHCI_INT_STATUS);
    // 割り込みステータスをクリアする
    sdhci_writel(host, stat, SDHCI_INT_STATUS);
    // 転送が成功の場合
    if (!ret) {
        // 該当しない
        if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) &&
                !is_aligned && (data->flags == MMC_DATA_READ)) {
            memcpy(data->dest, host->align_buffer, trans_bytes);
            trace("copy buf to dst");
        }
        // 成功リターン
        //printf("\n");
        return 0;
    }

    // 転送エラーがあった場合、コマンドラインとデータラインをリセットする
    //printf("16");
    sdhci_reset(host, SDHCI_RESET_CMD);
    //printf("17");
    sdhci_reset(host, SDHCI_RESET_DATA);
    //printf(" err ");
    if (stat & SDHCI_INT_TIMEOUT)
        return -ETIMEDOUT;
    else if (ret != -1)
        return ret;
    else
        return -ECOMM;
}

// チューニングを実行する
int sdhci_execute_tuning(struct mmc *mmc, unsigned int opcode)
{
    return cvi_general_execute_tuning(mmc, opcode);
}

//  sdhci固有のset_clock関数
int sdhci_set_clock(struct mmc *mmc, unsigned int clock)
{
    struct sdhci_host *host = mmc->priv;
    unsigned int div, clk = 0, timeout;

    trace("mmc%d : Set clock %d, host->max_clk %d", host->index, clock, host->max_clk);
    /* Wait max 20 ms */
    timeout = 200;
    // コマンドラインが開くのを待つ
    while (sdhci_readl(host, SDHCI_PRESENT_STATE) &
               (SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT)) {
        if (timeout == 0) {
            warn("Timeout to wait cmd & data inhibit");
            return -EBUSY;
        }

        timeout--;
        delayus(100);
    }

    // クロックを停止する: CLK_CT+_swRsT(0x2c).B{0:15]}
    sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
    // クロック停止が指定された場合は、リターン
    if (clock == 0)
        return 0;

    // バージョン3.00以上をサポートしている場合: 該当
    if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
        /*
         * ホストコントローラがプログラマブルクロックモードを
         * サポートしているかチェックする
         */
        // 該当しない
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
        // 該当する
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
    // 該当しない
    } else {
        /* Version 2.00 divisors must be a power of 2. */
        for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) {
            if ((host->max_clk / div) <= clock)
                break;
        }
        div >>= 1;
    }

    trace("mmc%d : clk div 0x%x", host->index, div);

    clk |= (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
    clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
        << SDHCI_DIVIDER_HI_SHIFT;
    clk |= SDHCI_CLOCK_INT_EN;

    trace("mmc%d : 0x2c clk reg 0x%x", host->index, clk);
    // CLK_CTL_SWRST.B[0:16]をセット, 特にB[8:15] FREQ_SEL
    sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

    /* Wait max 20 ms */
    timeout = 20;
    // クロックが安定するまで待つ
    while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
        & SDHCI_CLOCK_INT_STABLE)) {
        if (timeout == 0) {
            error("Internal clock never stabilised.");
            return -EBUSY;
        }
        timeout--;
        delayus(1000);
    }
    // SDクロックを有効にする
    clk |= SDHCI_CLOCK_CARD_EN;
    sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
    return 0;
}

// SDバスの電源をセットする
static void sdhci_set_power(struct sdhci_host *host, unsigned short power)
{
    uint8_t pwr = 0;

    // 1. HOST_CTL1_PWR_BG_WUP.SD_BUS_VOL_SEL[11:9] をセット
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
    // 2. power = -1 (電源オフを指定) の場合はSD Busの電源をオフ
    if (pwr == 0) {
        sdhci_writeb(host, 0, SDHCI_POWER_CONTROL);
        return;
    }

    // 3. HOST_CTL1_PWR_BG_WUP.SD_BUS_PWR[8] をセット
    pwr |= SDHCI_POWER_ON;

    // 4. HOST_CTL1_PWR_BG_WUP[9:15]を書き込む
    sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
}

// 17. UHSのタイミングをセットする
void sdhci_set_uhs_timing(struct sdhci_host *host)
{
    struct mmc *mmc = host->mmc;
    uint32_t reg;

    reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
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

    sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
}

// DAT0ラインがstateになるまで待つ
int sdhci_card_busy(struct mmc *mmc, int state, int timeout_us)
{
    struct sdhci_host *host = mmc->priv;
    int ret = -ETIMEDOUT;
    bool dat0_high;
    bool target_dat0_high = !!state;

    timeout_us = DIV_ROUND_UP(timeout_us, 10); /* check every 10 us. */
    while (timeout_us--) {
        // DAT0ラインの信号レベルを取得(PRESENT_STS[30])
        dat0_high = !!(sdhci_readl(host, SDHCI_PRESENT_STATE) & BIT(20));
        // 指定の自レベルであれば成功でリターン
        if (dat0_high == target_dat0_high) {
            ret = 0;
            break;
        }
        delayus(10);
    }
    return ret;
}

// 15. 信号電圧をセットする
void sdhci_set_voltage(struct mmc *mmc)
{
    struct sdhci_host *host = mmc->priv;
    uint32_t ctrl;

    ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);

    switch (mmc->signal_voltage) {
    case MMC_SIGNAL_VOLTAGE_330:
        if (IS_SD(mmc)) {
            // HOST_CTL2[19] : EN_18_SIGをクリア:  3.3Vにする
            ctrl &= ~SDHCI_CTRL_VDD_180;
            sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
        }

        /* Wait for 5ms */
        delayms(5);

        /* 3.3V regulator output should be stable within 5 ms */
        if (IS_SD(mmc)) {
            if (ctrl & SDHCI_CTRL_VDD_180) {
                warn("3.3V regulator output did not become stable");
                return;
            }
        }

        break;
    case MMC_SIGNAL_VOLTAGE_180:
        if (IS_SD(mmc)) {
            // EN_18_SIGんする: 1.8Vにする
            ctrl |= SDHCI_CTRL_VDD_180;
            sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
        }

        // cvi_sd_voltage_switch()を実行して信号電圧を切替える
        cvi_sd_voltage_switch(mmc);

        /* 1.8V regulator output has to be stable within 5 ms */
        if (IS_SD(mmc)) {
            ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
            if (!(ctrl & SDHCI_CTRL_VDD_180)) {
                warn("1.8V regulator output did not become stable");
                return;
            }
        }

        break;
    default:
        /* No signal voltage switch required */
        return;
    }
    sdhci_set_uhs_timing(host);
}

// SDバス: クロック、幅、スピードをセット
int sdhci_set_ios(struct mmc *mmc)
{
    uint32_t ctrl;
    struct sdhci_host *host = mmc->priv;
    bool no_hispd_bit = false;

    // SDバスクロックを停止、または開始
    if (mmc->clk_disable)
        sdhci_set_clock(mmc, 0);
    else if (mmc->clock != host->clock)
        sdhci_set_clock(mmc, mmc->clock);

    /* データバス幅をセット */
    // HOST_CTL1 (0x28)
    ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
    if (mmc->bus_width == 8) {
        ctrl &= ~SDHCI_CTRL_4BITBUS;
        if ((SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) ||
                (host->quirks & SDHCI_QUIRK_USE_WIDE8))
            ctrl |= SDHCI_CTRL_8BITBUS;
    } else {
        if ((SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) ||
                (host->quirks & SDHCI_QUIRK_USE_WIDE8))
            ctrl &= ~SDHCI_CTRL_8BITBUS;
        // 該当: 4ビット幅
        if (mmc->bus_width == 4)
            ctrl |= SDHCI_CTRL_4BITBUS;
        else
            ctrl &= ~SDHCI_CTRL_4BITBUS;
    }

    // 該当しない
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
    // 選択した速度モードを書き込む
    sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

    return 0;
}

// 初期化を行う
int sdhci_init(struct mmc *mmc)
{
    struct sdhci_host *host = mmc->priv;

    // 1. CMD/DAT両ラインをソフトウェアリセット
    sdhci_reset(host, SDHCI_RESET_ALL);

    // 2. カードの挿入をチェックして処理
    int present = cvi_get_cd(host);
    if (present == 1) {
        // 設定された使用可能最大電圧でSDバスの電源オン
        sdhci_set_power(host, generic_fls(mmc->cfg->voltages) - 1);
        delayms(5);
    } else if (present == 0) {
        // SDバスの電源オフ
        sdhci_set_power(host, (unsigned short)-1);
        delayms(30);
    }

    // 割り込みの要因のセットはするが、割り込み自体は発生させない
    // 1. SDコントローラが提供する割り込みのみ有効にする
    sdhci_writel(host, SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK,
        SDHCI_INT_ENABLE);
    // 2. すべての割り込みソースからの信号をマスク
    sdhci_writel(host, 0x0, SDHCI_SIGNAL_ENABLE);

    return 0;
}

// sdhciのプローブ処理
int sdhci_probe(struct mmc *mmc)
{
    // 初期化を行う
    return sdhci_init(mmc);
}

// カードは挿入されているか? : されていれば 1
int sdhci_get_cd(struct mmc *mmc)
{
    struct sdhci_host *host = mmc->priv;
    return cvi_get_cd(host);
}

#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))

// sdhciのセットアップ (dtsから取得したデータに基づいてセットアップ)
int sdhci_setup_cfg(struct mmc_config *cfg, struct sdhci_host *host,
        uint32_t f_max, uint32_t f_min)
{
    // caps: CAPABILITIES (0x40), caps_1: CAPABILITIES1 (0x44)
    uint32_t caps = sdhci_readl(host, SDHCI_CAPABILITIES);
    uint32_t caps_1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);
    trace("cap1: 0x%08x, cap2: 0x%08x", caps, caps_1);

    // SDHCI_CAN_DO_SDMA = 1 : SDMAを使用
    if ((caps & SDHCI_CAN_DO_SDMA)) {
        host->flags |= USE_SDMA;
    } else {
        warn("Your controller doesn't support SDMA!!");
    }

    if (host->quirks & SDHCI_QUIRK_REG32_RW)
        host->version =
            sdhci_readl(host, SDHCI_HOST_VERSION - 2) >> 16;
    else
        host->version = sdhci_readw(host, SDHCI_HOST_VERSION);

    cfg->name = host->name;

    /* クロックの逓倍がサポートされているかチェック */
    if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
        host->clk_mul = (caps_1 & SDHCI_CLOCK_MUL_MASK) >>
                SDHCI_CLOCK_MUL_SHIFT;
    }

    if (host->max_clk == 0) {
        if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300)
            // host->max_clk = 200
            host->max_clk = (caps & SDHCI_CLOCK_V3_BASE_MASK) >>
                SDHCI_CLOCK_BASE_SHIFT;
        else
            host->max_clk = (caps & SDHCI_CLOCK_BASE_MASK) >>
                SDHCI_CLOCK_BASE_SHIFT;
        host->max_clk *= 1000000;
        if (host->clk_mul)
            host->max_clk *= host->clk_mul;
        // host->mac_clk = 200 MHz
    }
    if (host->max_clk == 0) {
        error("Hardware doesn't specify base clock frequency");
        return -EINVAL;
    }
    // f_max = 200 MHz, cfg->f_max = 200 MHz, cfg->f_min = 400 KHz
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
    // 330/300/180 全部 1
    if (caps & SDHCI_CAN_VDD_330)
        cfg->voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
    if (caps & SDHCI_CAN_VDD_300)
        cfg->voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
    if (caps & SDHCI_CAN_VDD_180)
        cfg->voltages |= MMC_VDD_165_195;

    if (host->quirks & SDHCI_QUIRK_BROKEN_VOLTAGE)
        cfg->voltages |= host->voltages;

    // SDHCI_CAN_DO_HISPD = 1
    if (caps & SDHCI_CAN_DO_HISPD)
        cfg->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz;

    // dat lineは4bit
    cfg->host_caps |= MMC_MODE_4BIT;

    /* Since Host Controller Version3.0 */
    if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
        // SDHCI_CAN_DO_8BIT = 0
        if (!(caps & SDHCI_CAN_DO_8BIT))
            cfg->host_caps &= ~MMC_MODE_8BIT;
    }

    // 該当しない
    if (host->quirks & SDHCI_QUIRK_BROKEN_HISPD_MODE) {
        cfg->host_caps &= ~MMC_MODE_HS;
        cfg->host_caps &= ~MMC_MODE_HS_52MHz;
    }

    // 該当する
    if (!(cfg->voltages & MMC_VDD_165_195) ||
        (host->quirks & SDHCI_QUIRK_NO_1_8_V))
        caps_1 &= ~(SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
                SDHCI_SUPPORT_DDR50);

    // 該当する
    if (host->quirks & SDHCI_QUIRK_NO_1_8_V)
        caps_1 &= ~(SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
                SDHCI_SUPPORT_DDR50);

    // 該当しない
    if (caps_1 & (SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
              SDHCI_SUPPORT_DDR50))
        cfg->host_caps |= MMC_CAP(UHS_SDR12) | MMC_CAP(UHS_SDR25);

    // 該当しない
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
    // 該当しない
    if (caps_1 & SDHCI_SUPPORT_DDR50)
        cfg->host_caps |= MMC_CAP(UHS_DDR50);

    if (host->host_caps)
        cfg->host_caps |= host->host_caps;
    // cfg->b_max = 65535 (0xffff)
    cfg->b_max = 65535;

    return 0;
}
