#ifndef INC_EMMC_H
#define INC_EMMC_H

#include "config.h"

#include <common/types.h>
#include "cv180x.h"

#define MMC_CAP(mode)   (1 << mode)

#define SD_BLOCK_SIZE       512

/* 各種設定オプション */

// .8vはサポートしない
//#define SD_1_8V_SUPPORT

// High Speed/SDR25モードをサポート
#define SD_HIGH_SPEED

// 4ビットバス幅をサポート
#define SD_4BIT_DATA

// SD_INT_STSをポーリングで監視
#define SD_POLL_STATUS_REG

/*
 * Enable SDXC maximum performance mode.
 * Requires 150 mA power so disabled for now.
 */
//#define SDXC_MAXIMUM_PERFORMANCE

/* SDカード割り込みは無効: ポーリングを使用 */
#define SD_CARD_INTERRUPTS

// SD Clock Frequencies (in Hz)
#define SD_CLOCK_ID         400000
#define SD_CLOCK_NORMAL     25000000
#define SD_CLOCK_HIGH       50000000
#define SD_CLOCK_100        100000000
#define SD_CLOCK_208        208000000

/* Allow old sdhci versions (may cause errors), required for QEMU. */
#define SD_ALLOW_OLD_SDHCI

#define SD_ARG2             (SD0_BASE + 0x00)
#define SD_BLKSIZECNT       (SD0_BASE + 0x04)
#define SD_ARG1             (SD0_BASE + 0x08)
#define SD_CMDTM            (SD0_BASE + 0x0C)
#define SD_RESP0            (SD0_BASE + 0x10)
#define SD_RESP1            (SD0_BASE + 0x14)
#define SD_RESP2            (SD0_BASE + 0x18)
#define SD_RESP3            (SD0_BASE + 0x1C)
#define SD_DATA             (SD0_BASE + 0x20)
#define SD_PRESENT_STS           (SD0_BASE + 0x24)
#define     CMD_INHIBIT         (1 << 0)
#define     DAT_INHIBIT         (1 << 1)
#define SD_CONTROL0         (SD0_BASE + 0x28)
#define SD_CONTROL1         (SD0_BASE + 0x2C)
#define     SD_DIVIDER_SHIFT	    8
#define     SD_DIVIDER_HI_SHIFT	    6
#define     SD_DIV_MASK	            0xFF
#define     SD_DIV_MASK_LEN	        8
#define     SD_DIV_HI_MASK	        0x300
#define     SD_CLOCK_INT_EN	        (1 << 0)
#define     SD_CLOCK_INT_STABLE	    (1 << 1)
#define     SD_CLOCK_CARD_EN	    (1 << 2)
#define     SD_CLOCK_PLL_EN	        (1 << 3)
#define     SD_PROG_CLOCK_MODE	    (1 << 5)
#define     SD_RESET_ALL            (1 << 24)
#define     SD_RESET_CMD            (1 << 25)
#define     SD_RESET_DAT            (1 << 26)
#define SD_INT_STS          (SD0_BASE + 0x30)
#define SD_INT_STS_EN       (SD0_BASE + 0x34)
#define SD_INT_SIG_EN       (SD0_BASE + 0x38)
#define SD_CONTROL2         (SD0_BASE + 0x3C)
#define SD_CAPABILITIES_1   (SD0_BASE + 0x40)
#define     CAP1_TIMEOUT_CLK_MASK       0x0000003F
#define     CAP1_TIMEOUT_CLK_SHIFT      0
#define     CAP1_TIMEOUT_CLK_UNIT       0x00000080
#define     CAP1_CLOCK_BASE_MASK        0x00003F00
#define     CAP1_CLOCK_V3_BASE_MASK     0x0000FF00
#define     CAP1_CLOCK_BASE_SHIFT       8
#define     CAP1_MAX_BLOCK_MASK         0x00030000
#define     CAP1_MAX_BLOCK_SHIFT        16
#define     CAP1_CAN_DO_8BIT            (1 << 18)
#define     CAP1_CAN_DO_HISPD           (1 << 21)
#define     CAP1_CAN_VDD_330            (1 << 24)
#define     CAP1_CAN_VDD_300            (1 << 25)
#define     CAP1_CAN_VDD_180            (1 << 26)
#define     CAP1_MAX_DIV_SPEC_200       256
#define     CAP1_MAX_DIV_SPEC_300       2046
#define SD_CAPABILITIES_2   (SD0_BASE + 0x44)
#define     CAP2_CLK_MUL_MASK           0x00ff0000
#define     CAP2_CLK_MUL_SHIFT          16
#define     CAP2_SUPPORT_SDR50          (1 << 0)
#define     CAP2_SUPPORT_SDR104         (1 << 1)
#define     CAP2_SUPPORT_DDR50          (1 << 2)
#define SD_FORCE_IRPT       (SD0_BASE + 0x50)
#define SD_SLOTISR_VER      (SD0_BASE + 0xFC)

#define SD_CMD_INDEX(a)             ((a) << 24)
#define SD_CMD_TYPE_NORMAL          0
#define SD_CMD_TYPE_SUSPEND         (1 << 22)
#define SD_CMD_TYPE_RESUME          (2 << 22)
#define SD_CMD_TYPE_ABORT           (3 << 22)
#define SD_CMD_TYPE_MASK            (3 << 22)
#define SD_CMD_ISDATA               (1 << 21)
#define SD_CMD_IXCHK_EN             (1 << 20)
#define SD_CMD_CRCCHK_EN            (1 << 19)
#define SD_CMD_RSPNS_TYPE_NONE      0   // For no response
#define SD_CMD_RSPNS_TYPE_136       (1 << 16)   // For response R2 (with CRC), R3,4 (no CRC)
#define SD_CMD_RSPNS_TYPE_48        (2 << 16)   // For responses R1, R5, R6, R7 (with CRC)
#define SD_CMD_RSPNS_TYPE_48B       (3 << 16)   // For responses R1b, R5b (with CRC)
#define SD_CMD_RSPNS_TYPE_MASK      (3 << 16)
#define SD_CMD_MULTI_BLOCK          (1 << 5)
#define SD_CMD_DAT_DIR_HC           0
#define SD_CMD_DAT_DIR_CH           (1 << 4)
#define SD_CMD_AUTO_CMD_EN_NONE     0
#define SD_CMD_AUTO_CMD_EN_CMD12    (1 << 2)
#define SD_CMD_AUTO_CMD_EN_CMD23    (2 << 2)
#define SD_CMD_BLKCNT_EN            (1 << 1)
#define SD_CMD_DMA                  1

#define SD_ERR_CMD_TIMEOUT    0
#define SD_ERR_CMD_CRC        1
#define SD_ERR_CMD_END_BIT    2
#define SD_ERR_CMD_INDEX      3
#define SD_ERR_DATA_TIMEOUT   4
#define SD_ERR_DATA_CRC       5
#define SD_ERR_DATA_END_BIT   6
#define SD_ERR_CURRENT_LIMIT  7
#define SD_ERR_AUTO_CMD12     8
#define SD_ERR_ADMA           9
#define SD_ERR_TUNING        10
#define SD_ERR_RSVD          11

#define SD_ERR_MASK_CMD_TIMEOUT     (1 << (16 + SD_ERR_CMD_TIMEOUT))
#define SD_ERR_MASK_CMD_CRC         (1 << (16 + SD_ERR_CMD_CRC))
#define SD_ERR_MASK_CMD_END_BIT     (1 << (16 + SD_ERR_CMD_END_BIT))
#define SD_ERR_MASK_CMD_INDEX       (1 << (16 + SD_ERR_CMD_INDEX))
#define SD_ERR_MASK_DATA_TIMEOUT    (1 << (16 + SD_ERR_CMD_TIMEOUT))
#define SD_ERR_MASK_DATA_CRC        (1 << (16 + SD_ERR_CMD_CRC))
#define SD_ERR_MASK_DATA_END_BIT    (1 << (16 + SD_ERR_CMD_END_BIT))
#define SD_ERR_MASK_CURRENT_LIMIT   (1 << (16 + SD_ERR_CMD_CURRENT_LIMIT))
#define SD_ERR_MASK_AUTO_CMD12      (1 << (16 + SD_ERR_CMD_AUTO_CMD12))
#define SD_ERR_MASK_ADMA            (1 << (16 + SD_ERR_CMD_ADMA))
#define SD_ERR_MASK_TUNING          (1 << (16 + SD_ERR_CMD_TUNING))

#define SD_COMMAND_COMPLETE     1
#define SD_TRANSFER_COMPLETE    (1 << 1)
#define SD_BLOCK_GAP_EVENT      (1 << 2)
#define SD_DMA_INTERRUPT        (1 << 3)
#define SD_BUFFER_WRITE_READY   (1 << 4)
#define SD_BUFFER_READ_READY    (1 << 5)
#define SD_CARD_INSERTION       (1 << 6)
#define SD_CARD_REMOVAL         (1 << 7)
#define SD_CARD_INTERRUPT       (1 << 8)

#define SD_RESP_NONE        SD_CMD_RSPNS_TYPE_NONE
#define SD_RESP_R1          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R1b         (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R2          (SD_CMD_RSPNS_TYPE_136 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R3          SD_CMD_RSPNS_TYPE_48
#define SD_RESP_R4          SD_CMD_RSPNS_TYPE_136
#define SD_RESP_R5          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R5b         (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R6          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R7          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)

#define SD_DATA_READ        (SD_CMD_ISDATA | SD_CMD_DAT_DIR_CH)
#define SD_DATA_WRITE       (SD_CMD_ISDATA | SD_CMD_DAT_DIR_HC)

#define SD_CMD_RESERVED(a)  0xffffffff

#define SUCCESS(self)       (self->last_cmd_success)
#define FAIL(self)          (self->last_cmd_success == 0)

#define TIMEOUT(self)       (FAIL(self) && (self->last_error == 0))
#define CMD_TIMEOUT(self)   (FAIL(self) && (self->last_error & (1 << 16)))
#define CMD_CRC(self)       (FAIL(self) && (self->last_error & (1 << 17)))
#define CMD_END_BIT(self)   (FAIL(self) && (self->last_error & (1 << 18)))
#define CMD_INDEX(self)     (FAIL(self) && (self->last_error & (1 << 19)))
#define DATA_TIMEOUT(self)  (FAIL(self) && (self->last_error & (1 << 20)))
#define DATA_CRC(self)      (FAIL(self) && (self->last_error & (1 << 21)))
#define DATA_END_BIT(self)  (FAIL(self) && (self->last_error & (1 << 22)))
#define CURRENT_LIMIT(self) (FAIL(self) && (self->last_error & (1 << 23)))
#define ACMD12_ERROR(self)  (FAIL(self) && (self->last_error & (1 << 24)))
#define ADMA_ERROR(self)    (FAIL(self) && (self->last_error & (1 << 25)))
#define TUNING_ERROR(self)  (FAIL(self) && (self->last_error & (1 << 26)))

#define SD_VER_UNKNOWN      0
#define SD_VER_1            1
#define SD_VER_1_1          2
#define SD_VER_2            3
#define SD_VER_3            4
#define SD_VER_4            5

// The actual command indices
#define GO_IDLE_STATE           0
#define ALL_SEND_CID            2
#define SEND_RELATIVE_ADDR      3
#define SET_DSR                 4
#define IO_SET_OP_COND          5
#define SWITCH_FUNC             6
#define SELECT_CARD             7
#define DESELECT_CARD           7
#define SELECT_DESELECT_CARD    7
#define SEND_IF_COND            8
#define SEND_CSD                9
#define SEND_CID                10
#define VOLTAGE_SWITCH          11
#define STOP_TRANSMISSION       12
#define SEND_STATUS             13
#define GO_INACTIVE_STATE       15
#define SET_BLOCKLEN            16
#define READ_SINGLE_BLOCK       17
#define READ_MULTIPLE_BLOCK     18
#define SEND_TUNING_BLOCK       19
#define SPEED_CLASS_CONTROL     20
#define SET_BLOCK_COUNT         23
#define WRITE_BLOCK             24
#define WRITE_MULTIPLE_BLOCK    25
#define PROGRAM_CSD             27
#define SET_WRITE_PROT          28
#define CLR_WRITE_PROT          29
#define SEND_WRITE_PROT         30
#define ERASE_WR_BLK_START      32
#define ERASE_WR_BLK_END        33
#define ERASE                   38
#define LOCK_UNLOCK             42
#define APP_CMD                 55
#define GEN_CMD                 56

#define IS_APP_CMD              0x80000000
#define ACMD(a)                 (a | IS_APP_CMD)
#define SET_BUS_WIDTH           (6 | IS_APP_CMD)
#define SD_STATUS               (13 | IS_APP_CMD)
#define SEND_NUM_WR_BLOCKS      (22 | IS_APP_CMD)
#define SET_WR_BLK_ERASE_COUNT  (23 | IS_APP_CMD)
#define SD_SEND_OP_COND         (41 | IS_APP_CMD)
#define SET_CLR_CARD_DETECT     (42 | IS_APP_CMD)
#define SEND_SCR                (51 | IS_APP_CMD)


// 4バイト以外のアクセス
#define  SD_BUS_POWER_CNTL     (SD0_BASE + 0x29)
#define  SD_POWER_ON            0x01
#define  SD_POWER_180           0x0A
#define  SD_POWER_300           0x0C
#define  SD_POWER_330           0x0E

#define POWER_OFF_MODE          0
#define POWER_UP_MODE           1
#define POWER_ON_MODE           2
#define POWER_UNDEFINED_MODE    3

#define SD_CAN_VDD_330          (1 << 24)
#define SD_CAN_VDD_300          (1 << 25)
#define SD_CAN_VDD_180          (1 << 26)

#define SD_VDD_165_195         0x00000080    /*  7: VDD voltage 1.65 - 1.95 */
#define SD_VDD_20_21           0x00000100    /*  8: VDD voltage 2.0 ~ 2.1 */
#define SD_VDD_21_22           0x00000200    /*  9: VDD voltage 2.1 ~ 2.2 */
#define SD_VDD_22_23           0x00000400    /* 10: VDD voltage 2.2 ~ 2.3 */
#define SD_VDD_23_24           0x00000800    /* 11: VDD voltage 2.3 ~ 2.4 */
#define SD_VDD_24_25           0x00001000    /* 12: VDD voltage 2.4 ~ 2.5 */
#define SD_VDD_25_26           0x00002000    /* 13: VDD voltage 2.5 ~ 2.6 */
#define SD_VDD_26_27           0x00004000    /* 14: VDD voltage 2.6 ~ 2.7 */
#define SD_VDD_27_28           0x00008000    /* 15: VDD voltage 2.7 ~ 2.8 */
#define SD_VDD_28_29           0x00010000    /* 16: VDD voltage 2.8 ~ 2.9 */
#define SD_VDD_29_30           0x00020000    /* 17: VDD voltage 2.9 ~ 3.0 */
#define SD_VDD_30_31           0x00040000    /* 18: VDD voltage 3.0 ~ 3.1 */
#define SD_VDD_31_32           0x00080000    /* 19: VDD voltage 3.1 ~ 3.2 */
#define SD_VDD_32_33           0x00100000    /* 20: VDD voltage 3.2 ~ 3.3 */
#define SD_VDD_33_34           0x00200000    /* 21: VDD voltage 3.3 ~ 3.4 */
#define SD_VDD_34_35           0x00400000    /* 22: VDD voltage 3.4 ~ 3.5 */
#define SD_VDD_35_36           0x00800000    /* 23: VDD voltage 3.5 ~ 3.6 */

#define SD_HOST_VERSION     (SD0_BASE + 0xFE)
#define SPEC_VER_MASK       0x00FF
#define   SPEC_100              0
#define   SPEC_200              1
#define   SPEC_300              2

#define VENDOR_VER_MASK     0xFF00


#define  SD_INT_RESPONSE     (1 << 0)
#define  SD_INT_DATA_END     (1 << 1)
#define  SD_INT_DMA_END      (1 << 3)
#define  SD_INT_SPACE_AVAIL  (1 << 4)
#define  SD_INT_DATA_AVAIL   (1 << 5)
#define  SD_INT_CARD_INSERT  (1 << 6)
#define  SD_INT_CARD_REMOVE  (1 << 7)
#define  SD_INT_CARD_INT     (1 << 8)
#define  SD_INT_ERROR        (1 << 15)
#define  SD_INT_TIMEOUT      (1 << 16)
#define  SD_INT_CRC          (1 << 17)
#define  SD_INT_END_BIT      (1 << 18)
#define  SD_INT_INDEX        (1 << 19)
#define  SD_INT_DATA_TIMEOUT (1 << 20)
#define  SD_INT_DATA_CRC     (1 << 21)
#define  SD_INT_DATA_END_BIT (1 << 22)
#define  SD_INT_BUS_POWER    (1 << 23)
#define  SD_INT_ACMD12ERR    (1 << 24)
#define  SD_INT_ADMA_ERROR   (1 << 25)

#define  SD_INT_NORMAL_MASK  0x00007FFF
#define  SD_INT_ERROR_MASK   0xFFFF8000

#define  SD_INT_CMD_MASK    (SD_INT_RESPONSE | SD_INT_TIMEOUT | \
        SD_INT_CRC | SD_INT_END_BIT | SD_INT_INDEX)
#define  SD_INT_DATA_MASK    (SD_INT_DATA_END | SD_INT_DMA_END | \
        SD_INT_DATA_AVAIL | SD_INT_SPACE_AVAIL | \
        SD_INT_DATA_TIMEOUT | SD_INT_DATA_CRC | \
        SD_INT_DATA_END_BIT | SD_INT_ACMD12ERR | SD_INT_ADMA_ERROR)
#define SD_INT_ALL_MASK    ((unsigned int)-1)

#define SD_MAX_DIV_SPEC_200	        256
#define SD_MAX_DIV_SPEC_300	        2046

#define SD_GET_CLOCK_DIVIDER_FAIL    0xffffffff


enum bus_mode {
        MMC_LEGACY,     /* */
        MMC_HS,         /* MMC High Speed*/
        SD_HS,          /* SD High Speed*/
        MMC_HS_52,      /* MMC Hi*/
        MMC_DDR_52,
        UHS_SDR12,
        UHS_SDR25,
        UHS_SDR50,
        UHS_DDR50,
        UHS_SDR104,
        MMC_HS_200,
        MMC_HS_400,
        MMC_HS_400_ES,
        MMC_MODES_END
};

#define MMC_CAP(mode)       (1 << mode)
#define MMC_MODE_HS         (MMC_CAP(MMC_HS) | MMC_CAP(SD_HS))
#define MMC_MODE_HS_52MHz   MMC_CAP(MMC_HS_52)
#define MMC_MODE_DDR_52MHz  MMC_CAP(MMC_DDR_52)
#define MMC_MODE_HS200      MMC_CAP(MMC_HS_200)
#define MMC_MODE_HS400      MMC_CAP(MMC_HS_400)
#define MMC_MODE_HS400_ES   MC_CAP(MMC_HS_400_ES)

#define CAP_NONREMOVABLE    (1 << 14)
#define CAP_NEEDS_POLL      (1 << 15)
#define CAP_CD_ACTIVE_HIGH  (1 << 16)

#define CAP_MODE_8BIT       (1 << 30)
#define CAP_MODE_4BIT       (1 << 29)
#define CAP_MODE_1BIT       (1 << 28)
#define CAP_MODE_SPI        (1 << 27)


// SD構成レジスタ
struct tscr
{
  uint32_t  scr[2];         /* 生SCRデータを格納: BigEndian */
  uint32_t  sd_bus_widths;  /* SCRのSD_BUSWIDTHS*/
  int       sd_version;     /* SCRのバージョン: SD_SPEC/SD_SPEC3/SD_SPEC4/SD_SPEC5から作成 */
};

// sdhci_host構造体
struct sdhci_host {
    uint32_t    host_caps;  /* SDホストのケーパビリティ */
    uint32_t    version;    /* SD規格のサポートバージョン */
    uint32_t    max_clk;    /* 最大ベースクロック周波数 */
    uint32_t    clk_mul;    /* クロックの分周値 */
    uint32_t    f_min;      /* カードがサポートする最小クロック周波数 */
    uint32_t    f_max;      /* カードがサポートする最大クロック周波数 */
    uint32_t    b_max;      /* カードがサポートする最大ブロック長 */

    uint32_t    voltages;   /* 使用する電圧 */
    bool        vol_18v;    /* 現在電圧は1.8vか */
};

// emmc構造体
struct emmc {
    struct sdhci_host host;
    uint64_t    offset;     /* 現在のオフセット位置 */
    uint32_t    hci_ver;    /* SDホストコントローラのサポート規格バージョン */

    // was: struct SD_block_dev
    uint32_t    device_id[4];

    uint32_t    card_supports_sdhc; /* SDHCをサポートしているか */
    uint32_t    card_supports_hs;   /* High Speedをサポートしているか */
    uint32_t    card_supports_18v;  /* 1.8vをサポートしているか */
    uint32_t    card_ocr;           /* カードのOCRデータ */
    uint32_t    card_rca;           /* 現在選択しているカードのRCA */
    uint32_t    last_interrupt;     /* 最新の割り込み */
    uint32_t    last_error;         /* 最新のエラー */

    struct tscr scr;                /* カードのSCRを格納する構造体 */

    int         failed_voltage_switch;  /* 電圧切替に失敗 */

    uint32_t    last_cmd_reg;       /* 最新のコマンドレジスタ値 */
    uint32_t    last_cmd;           /* 最新のコマンド */
    uint32_t    last_cmd_success;   /* 最新のコマンド実行の成否 1: success, 0: failure *///
    uint32_t    last_r0;            /* 最新のレスsポンス[0] */
    uint32_t    last_r1;            /* 最新のレスsポンス[1] */
    uint32_t    last_r2;            /* 最新のレスsポンス[2] */
    uint32_t    last_r3;            /* 最新のレスsポンス[3] */

    void        *buf;               /* データ転送用のバッファへのポインタ */
    int         blocks_to_transfer; /* 転送するブロック数 */
    size_t      block_size;         /* ブロック長 */
    int         card_removal;       /* カードが挿入されていないか */
    uint8_t     pwr;                /* 現在のバスパワー値 */
};

#endif  /* ifndef INC_SD_H*/
