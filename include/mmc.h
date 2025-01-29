/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2008,2010 Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * Based (loosely) on the Linux code
 */

#ifndef INC_MMC_H
#define INC_MMC_H

#include <common/types.h>
#include <dma-mapping.h>

/* SD/MMC version bits; 8 flags, 8 major, 8 minor, 8 change */
#define SD_VERSION_SD       (1U << 31)
#define MMC_VERSION_MMC     (1U << 30)

#define MAKE_SDMMC_VERSION(a, b, c)    \
    ((((uint32_t)(a)) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(c))
#define MAKE_SD_VERSION(a, b, c)    \
    (SD_VERSION_SD | MAKE_SDMMC_VERSION(a, b, c))
#define MAKE_MMC_VERSION(a, b, c)    \
    (MMC_VERSION_MMC | MAKE_SDMMC_VERSION(a, b, c))

#define EXTRACT_SDMMC_MAJOR_VERSION(x)    \
    (((uint32_t)(x) >> 16) & 0xff)
#define EXTRACT_SDMMC_MINOR_VERSION(x)    \
    (((uint32_t)(x) >> 8) & 0xff)
#define EXTRACT_SDMMC_CHANGE_VERSION(x)    \
    ((uint32_t)(x) & 0xff)

#define SD_VERSION_3        MAKE_SD_VERSION(3, 0, 0)
#define SD_VERSION_2        MAKE_SD_VERSION(2, 0, 0)
#define SD_VERSION_1_0      MAKE_SD_VERSION(1, 0, 0)
#define SD_VERSION_1_10     MAKE_SD_VERSION(1, 10, 0)

#define MMC_VERSION_UNKNOWN MAKE_MMC_VERSION(0, 0, 0)
#define MMC_VERSION_1_2     MAKE_MMC_VERSION(1, 2, 0)
#define MMC_VERSION_1_4     MAKE_MMC_VERSION(1, 4, 0)
#define MMC_VERSION_2_2     MAKE_MMC_VERSION(2, 2, 0)
#define MMC_VERSION_3       MAKE_MMC_VERSION(3, 0, 0)
#define MMC_VERSION_4       MAKE_MMC_VERSION(4, 0, 0)
#define MMC_VERSION_4_1     MAKE_MMC_VERSION(4, 1, 0)
#define MMC_VERSION_4_2     MAKE_MMC_VERSION(4, 2, 0)
#define MMC_VERSION_4_3     MAKE_MMC_VERSION(4, 3, 0)
#define MMC_VERSION_4_4     MAKE_MMC_VERSION(4, 4, 0)
#define MMC_VERSION_4_41    MAKE_MMC_VERSION(4, 4, 1)
#define MMC_VERSION_4_5     MAKE_MMC_VERSION(4, 5, 0)
#define MMC_VERSION_5_0     MAKE_MMC_VERSION(5, 0, 0)
#define MMC_VERSION_5_1     MAKE_MMC_VERSION(5, 1, 0)

#define MMC_CAP(mode)       (1 << mode)
#define MMC_MODE_HS         (MMC_CAP(MMC_HS) | MMC_CAP(SD_HS))
#define MMC_MODE_HS_52MHz   MMC_CAP(MMC_HS_52)
#define MMC_MODE_DDR_52MHz  MMC_CAP(MMC_DDR_52)
#define MMC_MODE_HS200      MMC_CAP(MMC_HS_200)
#define MMC_MODE_HS400      MMC_CAP(MMC_HS_400)
#define MMC_MODE_HS400_ES   MMC_CAP(MMC_HS_400_ES)

#define MMC_CAP_NONREMOVABLE    BIT(14)
#define MMC_CAP_NEEDS_POLL      BIT(15)
#define MMC_CAP_CD_ACTIVE_HIGH  BIT(16)

#define MMC_MODE_8BIT       BIT(30)
#define MMC_MODE_4BIT       BIT(29)
#define MMC_MODE_1BIT       BIT(28)
#define MMC_MODE_SPI        BIT(27)

#define SD_DATA_4BIT    0x00040000

#define IS_SD(x)    ((x)->version & SD_VERSION_SD)
#define IS_MMC(x)   ((x)->version & MMC_VERSION_MMC)

#define MMC_DATA_READ       1
#define MMC_DATA_WRITE      2

// コマンドインデックス
#define MMC_CMD_GO_IDLE_STATE           0
#define MMC_CMD_SEND_OP_COND            1   // ONLY MMC
#define MMC_CMD_ALL_SEND_CID            2
#define MMC_CMD_SET_RELATIVE_ADDR       3
#define MMC_CMD_SET_DSR                 4
#define MMC_CMD_SWITCH                  6
#define MMC_CMD_SELECT_CARD             7
#define MMC_CMD_SEND_EXT_CSD            8
#define MMC_CMD_SEND_CSD                9
#define MMC_CMD_SEND_CID                10
#define MMC_CMD_STOP_TRANSMISSION       12
#define MMC_CMD_SEND_STATUS             13
#define MMC_CMD_SET_BLOCKLEN            16
#define MMC_CMD_READ_SINGLE_BLOCK       17
#define MMC_CMD_READ_MULTIPLE_BLOCK     18
#define MMC_CMD_SEND_TUNING_BLOCK       19
#define MMC_CMD_SEND_TUNING_BLOCK_HS200 21  // NOT USE
#define MMC_CMD_SET_BLOCK_COUNT         23
#define MMC_CMD_WRITE_SINGLE_BLOCK      24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK    25
#define MMC_CMD_ERASE_WR_BLK_START      32  // ONLY SD
#define MMC_CMD_ERASE_WR_BLK_END        33  // ONLY SD
#define MMC_CMD_ERASE_GROUP_START       35  // ONLY MMC
#define MMC_CMD_ERASE_GROUP_END         36  // ONLY MMC
#define MMC_CMD_ERASE                   38
#define MMC_CMD_APP_CMD                 55
#define MMC_CMD_SPI_READ_OCR            58  // NOT USE
#define MMC_CMD_SPI_CRC_ON_OFF          59  // NOT USE
#define MMC_CMD_RES_MAN                 62  // NOT USE

#define MMC_CMD62_ARG1            0xefac62ec
#define MMC_CMD62_ARG2            0xcbaea7

#define SD_CMD_SEND_RELATIVE_ADDR   3
#define SD_CMD_SWITCH_FUNC          6
#define SD_CMD_SEND_IF_COND         8
#define SD_CMD_SWITCH_UHS18V        11

#define SD_CMD_APP_SET_BUS_WIDTH    6
#define SD_CMD_APP_SD_STATUS        13
#define SD_CMD_ERASE_WR_BLK_START   32
#define SD_CMD_ERASE_WR_BLK_END     33
#define SD_CMD_APP_SEND_OP_COND     41
#define SD_CMD_APP_SEND_SCR         51

// コマンドはTuningコマンドか?
static inline bool mmc_is_tuning_cmd(unsigned int cmdidx)
{
    if ((cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) ||
        (cmdidx == MMC_CMD_SEND_TUNING_BLOCK))
        return true;
    return false;
}

/* SCR definitions in different words */
#define SD_HIGHSPEED_BUSY       0x00020000
#define SD_HIGHSPEED_SUPPORTED  0x00020000

#define UHS_SDR12_BUS_SPEED     0
#define HIGH_SPEED_BUS_SPEED    1
#define UHS_SDR25_BUS_SPEED     1
#define UHS_SDR50_BUS_SPEED     2
#define UHS_SDR104_BUS_SPEED    3
#define UHS_DDR50_BUS_SPEED     4

#define SD_MODE_UHS_SDR12   BIT(UHS_SDR12_BUS_SPEED)
#define SD_MODE_UHS_SDR25   BIT(UHS_SDR25_BUS_SPEED)
#define SD_MODE_UHS_SDR50   BIT(UHS_SDR50_BUS_SPEED)
#define SD_MODE_UHS_SDR104  BIT(UHS_SDR104_BUS_SPEED)
#define SD_MODE_UHS_DDR50   BIT(UHS_DDR50_BUS_SPEED)

#define OCR_BUSY                0x80000000
#define OCR_HCS                 0x40000000
#define OCR_S18R                0x1000000
#define OCR_VOLTAGE_MASK        0x007FFF80
#define OCR_ACCESS_MODE         0x60000000

#define MMC_ERASE_ARG           0x00000000
#define MMC_SECURE_ERASE_ARG    0x80000000
#define MMC_TRIM_ARG            0x00000001
#define MMC_DISCARD_ARG         0x00000003
#define MMC_SECURE_TRIM1_ARG    0x80000001
#define MMC_SECURE_TRIM2_ARG    0x80008000

#define MMC_STATUS_MASK         (~0x0206BF7F)
#define MMC_STATUS_SWITCH_ERROR (1 << 7)
#define MMC_STATUS_RDY_FOR_DATA (1 << 8)
#define MMC_STATUS_CURR_STATE   (0xf << 9)
#define MMC_STATUS_ERROR        (1 << 19)

#define MMC_STATE_PRG           (7 << 9)
#define MMC_STATE_TRANS         (4 << 9)

#define MMC_VDD_165_195     0x00000080  /* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21       0x00000100  /* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22       0x00000200  /* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23       0x00000400  /* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24       0x00000800  /* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25       0x00001000  /* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26       0x00002000  /* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27       0x00004000  /* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28       0x00008000  /* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29       0x00010000  /* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30       0x00020000  /* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31       0x00040000  /* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32       0x00080000  /* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33       0x00100000  /* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34       0x00200000  /* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35       0x00400000  /* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36       0x00800000  /* VDD voltage 3.5 ~ 3.6 */

#define MMC_SWITCH_MODE_CMD_SET  0x00 /* Change the command set */
#define MMC_SWITCH_MODE_SET_BITS 0x01 /* Set bits in EXT_CSD byte
                        addressed by index which are
                        1 in value field */
#define MMC_SWITCH_MODE_CLEAR_BITS  0x02 /* Clear bits in EXT_CSD byte
                        addressed by index, which are
                        1 in value field */
#define MMC_SWITCH_MODE_WRITE_BYTE  0x03 /* Set target byte to value */

#define SD_SWITCH_CHECK     0
#define SD_SWITCH_SWITCH    1

/*
 * EXT_CSD fields
 */
#define EXT_CSD_ENH_START_ADDR      136    /* R/W */
#define EXT_CSD_ENH_SIZE_MULT       140    /* R/W */
#define EXT_CSD_GP_SIZE_MULT        143    /* R/W */
#define EXT_CSD_PARTITION_SETTING   155    /* R/W */
#define EXT_CSD_PARTITIONS_ATTRIBUTE    156    /* R/W */
#define EXT_CSD_MAX_ENH_SIZE_MULT   157    /* R */
#define EXT_CSD_PARTITIONING_SUPPORT    160    /* RO */
#define EXT_CSD_RST_N_FUNCTION      162    /* R/W */
#define EXT_CSD_BKOPS_EN            163    /* R/W & R/W/E */
#define EXT_CSD_WR_REL_PARAM        166    /* R */
#define EXT_CSD_WR_REL_SET          167    /* R/W */
#define EXT_CSD_RPMB_MULT           168    /* RO */
#define EXT_CSD_USER_WP             171    /* R/W & R/W/C_P & R/W/E_P */
#define EXT_CSD_BOOT_WP             173    /* R/W & R/W/C_P */
#define EXT_CSD_BOOT_WP_STATUS      174    /* R */
#define EXT_CSD_ERASE_GROUP_DEF     175    /* R/W */
#define EXT_CSD_BOOT_BUS_WIDTH      177
#define EXT_CSD_PART_CONF           179    /* R/W */
#define EXT_CSD_BUS_WIDTH           183    /* R/W */
#define EXT_CSD_STROBE_SUPPORT      184    /* R/W */
#define EXT_CSD_HS_TIMING           185    /* R/W */
#define EXT_CSD_REV                 192    /* RO */
#define EXT_CSD_CARD_TYPE           196    /* RO */
#define EXT_CSD_PART_SWITCH_TIME    199    /* RO */
#define EXT_CSD_SEC_CNT             212    /* RO, 4 bytes */
#define EXT_CSD_HC_WP_GRP_SIZE      221    /* RO */
#define EXT_CSD_HC_ERASE_GRP_SIZE   224    /* RO */
#define EXT_CSD_BOOT_MULT           226    /* RO */
#define EXT_CSD_GENERIC_CMD6_TIME   248     /* RO */
#define EXT_CSD_BKOPS_SUPPORT       502    /* RO */

/*
 * EXT_CSD field definitions
 */

#define EXT_CSD_CMD_SET_NORMAL      (1 << 0)
#define EXT_CSD_CMD_SET_SECURE      (1 << 1)
#define EXT_CSD_CMD_SET_CPSECURE    (1 << 2)

#define EXT_CSD_CARD_TYPE_26    (1 << 0)    /* Card can run at 26MHz */
#define EXT_CSD_CARD_TYPE_52    (1 << 1)    /* Card can run at 52MHz */
#define EXT_CSD_CARD_TYPE_DDR_1_8V  (1 << 2)
#define EXT_CSD_CARD_TYPE_DDR_1_2V  (1 << 3)
#define EXT_CSD_CARD_TYPE_DDR_52    (EXT_CSD_CARD_TYPE_DDR_1_8V \
                    | EXT_CSD_CARD_TYPE_DDR_1_2V)

#define EXT_CSD_CARD_TYPE_HS200_1_8V    BIT(4)    /* Card can run at 200MHz */
                        /* SDR mode @1.8V I/O */
#define EXT_CSD_CARD_TYPE_HS200_1_2V    BIT(5)    /* Card can run at 200MHz */
                        /* SDR mode @1.2V I/O */
#define EXT_CSD_CARD_TYPE_HS200        (EXT_CSD_CARD_TYPE_HS200_1_8V | \
                     EXT_CSD_CARD_TYPE_HS200_1_2V)
#define EXT_CSD_CARD_TYPE_HS400_1_8V    BIT(6)
#define EXT_CSD_CARD_TYPE_HS400_1_2V    BIT(7)
#define EXT_CSD_CARD_TYPE_HS400        (EXT_CSD_CARD_TYPE_HS400_1_8V | \
                     EXT_CSD_CARD_TYPE_HS400_1_2V)

#define EXT_CSD_BUS_WIDTH_1     0    /* Card is in 1 bit mode */
#define EXT_CSD_BUS_WIDTH_4     1    /* Card is in 4 bit mode */
#define EXT_CSD_BUS_WIDTH_8     2    /* Card is in 8 bit mode */
#define EXT_CSD_DDR_BUS_WIDTH_4 5    /* Card is in 4 bit DDR mode */
#define EXT_CSD_DDR_BUS_WIDTH_8 6    /* Card is in 8 bit DDR mode */
#define EXT_CSD_DDR_FLAG        BIT(2)    /* Flag for DDR mode */
#define EXT_CSD_BUS_WIDTH_STROBE BIT(7)    /* Enhanced strobe mode */

#define EXT_CSD_TIMING_LEGACY   0    /* no high speed */
#define EXT_CSD_TIMING_HS       1    /* HS */
#define EXT_CSD_TIMING_HS200    2    /* HS200 */
#define EXT_CSD_TIMING_HS400    3    /* HS400 */
#define EXT_CSD_DRV_STR_SHIFT   4    /* Driver Strength shift */

#define EXT_CSD_BOOT_ACK_ENABLE             (1 << 6)
#define EXT_CSD_BOOT_PARTITION_ENABLE       (1 << 3)
#define EXT_CSD_PARTITION_ACCESS_ENABLE     (1 << 0)
#define EXT_CSD_PARTITION_ACCESS_DISABLE    (0 << 0)

#define EXT_CSD_BOOT_ACK(x)         (x << 6)
#define EXT_CSD_BOOT_PART_NUM(x)    (x << 3)
#define EXT_CSD_PARTITION_ACCESS(x) (x << 0)

#define EXT_CSD_EXTRACT_BOOT_ACK(x) (((x) >> 6) & 0x1)
#define EXT_CSD_EXTRACT_BOOT_PART(x) (((x) >> 3) & 0x7)
#define EXT_CSD_EXTRACT_PARTITION_ACCESS(x) ((x) & 0x7)

#define EXT_CSD_BOOT_BUS_WIDTH_MODE(x)  (x << 3)
#define EXT_CSD_BOOT_BUS_WIDTH_RESET(x) (x << 2)
#define EXT_CSD_BOOT_BUS_WIDTH_WIDTH(x) (x)

#define EXT_CSD_PARTITION_SETTING_COMPLETED (1 << 0)

#define EXT_CSD_ENH_USR     (1 << 0)    /* user data area is enhanced */
#define EXT_CSD_ENH_GP(x)   (1 << ((x)+1))  /* GP part (x+1) is enhanced */

#define EXT_CSD_HS_CTRL_REL (1 << 0)    /* host controlled WR_REL_SET */

#define EXT_CSD_WR_DATA_REL_USR     (1 << 0)    /* user data area WR_REL */
#define EXT_CSD_WR_DATA_REL_GP(x)   (1 << ((x)+1))    /* GP part (x+1) WR_REL */

#define R1_ILLEGAL_COMMAND  (1 << 22)
#define R1_APP_CMD          (1 << 5)

#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136     (1 << 1)        /* 136 bit response */
#define MMC_RSP_CRC     (1 << 2)        /* expect valid crc */
#define MMC_RSP_BUSY    (1 << 3)        /* card may send busy */
#define MMC_RSP_OPCODE  (1 << 4)        /* response contains opcode */

#define MMC_RSP_NONE    (0)
#define MMC_RSP_R1      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R1b     (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE| \
            MMC_RSP_BUSY)
#define MMC_RSP_R2      (MMC_RSP_PRESENT|MMC_RSP_136|MMC_RSP_CRC)
#define MMC_RSP_R3      (MMC_RSP_PRESENT)
#define MMC_RSP_R4      (MMC_RSP_PRESENT)
#define MMC_RSP_R5      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R6      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R7      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)

#define MMCPART_NOAVAILABLE (0xff)
#define PART_ACCESS_MASK    (0x7)
#define PART_SUPPORT        (0x1)
#define ENHNCD_SUPPORT      (0x2)
#define PART_ENH_ATTRIB     (0x1f)

#define MMC_QUIRK_RETRY_SEND_CID        BIT(0)
#define MMC_QUIRK_RETRY_SET_BLOCKLEN    BIT(1)
#define MMC_QUIRK_RETRY_APP_CMD         BIT(2)

enum mmc_voltage {
    MMC_SIGNAL_VOLTAGE_000 = 0,
    MMC_SIGNAL_VOLTAGE_120 = 1,
    MMC_SIGNAL_VOLTAGE_180 = 2,
    MMC_SIGNAL_VOLTAGE_330 = 4,
};

#define MMC_ALL_SIGNAL_VOLTAGE (MMC_SIGNAL_VOLTAGE_120 |\
                MMC_SIGNAL_VOLTAGE_180 |\
                MMC_SIGNAL_VOLTAGE_330)

/* MMCの最大ブロックサイズ */
#define MMC_MAX_BLOCK_LEN   512

/* MMCの物理パーティション数.
 * MMC v4.4ではブートパーティション (2)と
 * 汎用パーティション (4) で構成される。
 */
#define MMC_NUM_BOOT_PARTITION  2
#define MMC_PART_RPMB           3       /* RPMB partition number */

/* timing specification used */
#define MMC_TIMING_LEGACY       0
#define MMC_TIMING_MMC_HS       1
#define MMC_TIMING_SD_HS        2
#define MMC_TIMING_UHS_SDR12    3
#define MMC_TIMING_UHS_SDR25    4
#define MMC_TIMING_UHS_SDR50    5
#define MMC_TIMING_UHS_SDR104   6
#define MMC_TIMING_UHS_DDR50    7
#define MMC_TIMING_MMC_DDR52    8
#define MMC_TIMING_MMC_HS200    9
#define MMC_TIMING_MMC_HS400    10

// コマンド構造体
struct mmc_cmd {
    unsigned short cmdidx;      // コマンドインデックス
    unsigned int resp_type;     // レスポンス種別
    unsigned int cmdarg;        // コマンド引数
    unsigned int response[4];   // レスポンスを格納
};

// データ構造体
struct mmc_data {
    union {
        char *dest;         // 受信バッファ
        const char *src;    // 送信バッファ
    };
    unsigned int flags;     // フラグ
    unsigned int blocks;    // ブロック数
    unsigned int blocksize; // ブロックサイズ
};

/* forward decl. */
struct mmc;

// MMC構成データ構造体: dtcから読み込んだ構成データを格納する
struct mmc_config {
    const char *name;           // 名前
    unsigned int host_caps;     // ホストの機能
    unsigned int voltages;      // 電圧
    unsigned int f_min;         // 最低周波数
    unsigned int f_max;         // 最高周波数
    unsigned int b_max;         // 最大ブロック
    unsigned char part_type;    //
};

// SD Status構造体: SDカード独自機能に関する情報を
struct sd_ssr {
    unsigned int au;            /* AUサイズ（セクタ単位） */
    unsigned int erase_timeout; /* eraseタイムアウト（ミリ秒単位） */
    unsigned int erase_offset;  /* eraseオフセット（ミリ秒単位） */
};

// バスモード
enum bus_mode {
    MMC_LEGACY,         // SD
    MMC_HS,
    SD_HS,              // SD
    MMC_HS_52,
    MMC_DDR_52,
    UHS_SDR12,          // SD
    UHS_SDR25,          // SD
    UHS_SDR50,          // SD
    UHS_DDR50,          // SD
    UHS_SDR104,         // SD
    MMC_HS_200,
    MMC_HS_400,
    MMC_HS_400_ES,
    MMC_MODES_END
};

const char *mmc_mode_name(enum bus_mode mode);
void mmc_dump_capabilities(const char *text, unsigned int caps);

// MMCのバスモードはDDR (Double Data Rate）か : UHS_DDR50は該当しない
static inline bool mmc_is_mode_ddr(enum bus_mode mode)
{
    if (mode == MMC_DDR_52 || mode == UHS_DDR50)
        return true;
    else if (mode == MMC_HS_400 || mode == MMC_HS_400_ES)
        return true;
    else
        return false;
}

// SDのみ : UHSモードサポートビット
#define UHS_CAPS (MMC_CAP(UHS_SDR12) | MMC_CAP(UHS_SDR25) | \
          MMC_CAP(UHS_SDR50) | MMC_CAP(UHS_SDR104) | \
          MMC_CAP(UHS_DDR50))

// SDのみ : UHSをサポートしているか
static inline bool supports_uhs(unsigned int caps)
{
    return (caps & UHS_CAPS) ? true : false;
}

/*
 * CONFIG_DM_MMCが有効の場合、struct mmcはmmc_get_mmc_dev()を
 * 使ってmmcデバイスからアクセスできる
 *
 * TODO struct mmc should be in mmc_private but it's hard to fix right now
 */
struct mmc {
    struct mmc_config *cfg;      /* dtsからセットするconfiguration */
    unsigned int version;       /* MMCバージョン*/
    void *priv;                 /* sdhci_hostへのポインタ */
    unsigned int has_init;      /* 1: 初期化済みである */
    int high_capacity;          /* 1: High Capacity */
    bool clk_disable;           /* true: クロックが無効 */
    unsigned int bus_width;     /* バス幅 */
    unsigned int clock;         /* クロック */
    unsigned int saved_clock;   /* 保存済クロック*/
    enum mmc_voltage signal_voltage;    /* シグナル電圧 */
    unsigned int card_caps;     /* カードの機能 */
    unsigned int host_caps;     /* ホストの機能 */
    unsigned int ocr;           /* OCRを格納 */
    unsigned int dsr;           /* DSRを格納*/
    unsigned int dsr_imp;       /* 1: DSRを実装 */
    unsigned int scr[2];        /* SCRを格納 */
    unsigned int csd[4];        /* CSDを格納 */
    unsigned int cid[4];        /* CIDを格納 */
    unsigned short rca;         /* RCAを格納 */
    unsigned int tran_speed;    /* 転送速度 */
    unsigned int legacy_speed;  /* レガシーモード用の転送速度 */
    unsigned int read_bl_len;   /* readデータブロックの最大長 */
    unsigned int write_bl_len;  /* writeデータブロックの最大長 */
    unsigned int erase_grp_size;    /* SDでは常に1 : High-capacity erase unit size (512-byteセクタ単位） */
    struct sd_ssr    ssr;       /* SDのみ : SSR(SD status)を格納 */

    char op_cond_pending;   /* 1: op_condコマンドの処理待ち */
    char init_in_progress;  /* 1: mmc_start_init()を実行済み */
    char preinit;           /* 1: できるだけ早くinitを開始する */
    int ddr_mode;           /* 1: DDRモード */
    enum mmc_voltage current_voltage;   /* 現在使用中の電圧 */
    enum bus_mode selected_mode;        /* 現在使用中のバスモード */
    enum bus_mode best_mode; /* 最大バンド幅でサポートされているいるモード */
    uint32_t quirks;        /* 1: quirksを使用 */
};

static inline uint64_t div_u64_rem(uint64_t dividend, uint32_t divisor, uint32_t *remainder)
{
    *remainder = dividend % divisor;
    return dividend / divisor;
}

static inline enum dma_data_direction mmc_get_dma_dir(struct mmc_data *data)
{
    return data->flags & MMC_DATA_WRITE ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
}

#define MMC_CLK_ENABLE        false
#define MMC_CLK_DISABLE        true

// 公開関数 : defs.hに移動すること

int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data);
int mmc_send_status(struct mmc *mmc, unsigned int *status);
int mmc_poll_for_busy(struct mmc *mmc, int timeout_ms);
int mmc_set_blocklen(struct mmc *mmc, int len);
int mmc_send_tuning(struct mmc *mmc, uint32_t opcode, int *cmd_error);
int mmc_set_clock(struct mmc *mmc, unsigned int clock, bool disable);
int mmc_voltage_to_mv(enum mmc_voltage voltage);
int mmc_initialize(struct mmc *mmc);
int mmc_wait_dat0(struct mmc *mmc, int state, int timeout_us);

unsigned long mmc_bread_mbr(struct mmc *mmc, void *dst);
unsigned long mmc_bread(struct mmc *mmc, lbaint_t start, lbaint_t blkcnt, void *dst);
unsigned long mmc_berase(struct mmc *mmc, lbaint_t start, lbaint_t blkcnt);
unsigned long mmc_bwrite(struct mmc *mmc, lbaint_t start, lbaint_t blkcnt, const void *src);

// treaceを出力する場合は1にする
//#define MMC_TRACE   1
#undef MMC_TRACE

#ifdef MMC_TRACE
void mmmc_trace_before_send(struct mmc *mmc, struct mmc_cmd *cmd);
void mmmc_trace_after_send(struct mmc *mmc, struct mmc_cmd *cmd, int ret);
void mmc_trace_state(struct mmc *mmc, struct mmc_cmd *cmd);
#else
static inline void mmmc_trace_before_send(struct mmc *mmc, struct mmc_cmd *cmd)
{
}

static inline void mmmc_trace_after_send(struct mmc *mmc, struct mmc_cmd *cmd,
                     int ret)
{
}

static inline void mmc_trace_state(struct mmc *mmc, struct mmc_cmd *cmd)
{
}
#endif

#endif /* INC_MMC_H */
