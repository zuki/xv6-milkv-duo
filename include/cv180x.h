#ifndef INC_CV180X_REG_H
#define INC_CV180X_REG_H

#define MMIO_BASE       0x03000000
#define SYSCTL_BASE     0x03000000
#define PINMUX_BASE     0x03001000
#define CLKGEN_BASE     0x03002000
#define RESET_BASE      0x03003000
#define WATCHDOG_BASE   0x03010000
#define TIMVER_BASE     0x030A0000
#define ETH0_BASE       0x04070000
#define RTC_BASE        0x05000000
#define SD0_BASE        0x04310000
#define SD1_BASE        0x04320000
#define USB_BASE        0x04340000

#define SOFT_RSTN_0         (RESET_BASE + 0x00)
#define SOFT_RSTN_1         (RESET_BASE + 0x04)
#define SOFT_RSTN_2         (RESET_BASE + 0x08)
#define SOFT_RSTN_3         (RESET_BASE + 0x0c)
#define SOFT_CPUAC_RSTN     (RESET_BASE + 0x20)
#define SOFT_CPU_RSTN       (RESET_BASE + 0x24)

/*
 * General purpose registers: 汎用レジスタ: 情報なし
 */
#define GP_REG0     (SYSCTL_BASE + 0x80)
#define GP_REG1     (SYSCTL_BASE + 0x84)
#define GP_REG2     (SYSCTL_BASE + 0x88)
#define GP_REG3     (SYSCTL_BASE + 0x8C)
#define GP_REG4     (SYSCTL_BASE + 0x90)
#define GP_REG5     (SYSCTL_BASE + 0x94)
#define GP_REG6     (SYSCTL_BASE + 0x98)
#define GP_REG7     (SYSCTL_BASE + 0x9C)
#define GP_REG8     (SYSCTL_BASE + 0xA0)
#define GP_REG9     (SYSCTL_BASE + 0xA4)
#define GP_REG10    (SYSCTL_BASE + 0xA8)

/* System Control レジスタ */
#define SYSCTL_CONF_INFO                (SYSCTL_BASE + 0x4)
#define BIT_SYSCTL_CONF_INFO_VBUS           (1 << 9)
#define SYSCTL_USB_CTRSTS               (SYSCTL_BASE + 0x38)
#define SYSCTL_USB_PHY_CTRL             (SYSCTL_BASE + 0x48)
#define BIT_SYSCTL_USB_PHY_CTRL_EXTVBUS     (1 << 0)
#define USB_PHY_ID_OVERRIDE_ENABLE          (1 << 5)
#define USB_PHY_ID_VALUE                    (1 << 7)
#define SYSCTL_DDR_ADDR_MODE            (SYSCTL_BASE + 0x64)
#define SYSCTL_USB_ECO                  (SYSCTL_BASE + 0xB4)
#define BIT_SYSCTL_USB_ECO_RX_FLUSH         0x80

/* rst: リセット */
#define SOFT_RSTN_0                     (RESET_BASE + 0x00)
#define BIT_SYSCTL_SOFT_RST_USB            (1 << 11)
/* datasheetと違う: 14: NAD, 12: ETH0 */
#define BIT_SYSCTL_SOFT_RST_SDIO           (1 << 14)
#define BIT_SYSCTL_SOFT_RST_NAND           (1 << 12)

/* irq: 割り込み種別 */
#define IRQ_LEVEL               0
#define IRQ_EDGE                3

/* RTC */
/* RTC_CTRLレジスタ */
#define RTC_CTRL_BASE       (RTC_BASE + 0x25000)
#define RTC_CTRL0_UNLOCKKEY     (RTC_CTRL_BASE + 0x04)
#define RTC_CTRL0               (RTC_CTRL_BASE + 0x08)
#define RTC_CTRL0_STATUS0       (RTC_CTRL_BASE + 0x0C)
#define RTC_RST_CTRL            (RTC_CTRL_BASE + 0x18)
#define RTC_FC_COARSE_EN        (RTC_CTRL_BASE + 0x40)
#define RTC_FC_COARSE_CAL       (RTC_CTRL_BASE + 0x44)
#define RTC_FC_FINE_EN          (RTC_CTRL_BASE + 0x48)
#define RTC_FC_FINE_PERIOD      (RTC_CTRL_BASE + 0x4C)
#define RTC_FC_FINE_CAL         (RTC_CTRL_BASE + 0x50)
#define RTC_POR_RST_CTRL        (RTC_CTRL_BASE + 0xac)

/* RTC_COREレジスタ */
#define RTC_CORE_BASE       (RTC_BASE + 0x26000)
#define RTC_ANA_CALIB           (RTC_CORE_BASE + 0x00)
#define RTC_SEC_PULST_GEN       (RTC_CORE_BASE + 0x04)
#define RTC_SET_SEC_CNTR_VALUE  (RTC_CORE_BASE + 0x10)
#define RTC_SET_SEC_CNTR_TRIG   (RTC_CORE_BASE + 0x14)
#define RTC_SEC_CNTR_VALUE      (RTC_CORE_BASE + 0x18)
#define RTC_INFO0               (RTC_CORE_BASE + 0x1C)
#define RTC_POR_DB_MAGIC_KEY    (RTC_CORE_BASE + 0x68)
#define RTC_EN_PWR_WAKEUP       (RTC_CORE_BASE + 0xBC)
#define RTC_EN_SHDN_REQ         (RTC_CORE_BASE + 0xC0)
#define RTC_EN_PWR_CYC_REQ      (RTC_CORE_BASE + 0xC8)
#define RTC_EN_WARM_RST_REQ     (RTC_CORE_BASE + 0xCC)
#define RTC_EN_PWR_VBAT_DET     (RTC_CORE_BASE + 0xD0)
#define RTC_EN_WDT_RST_REQ      (RTC_CORE_BASE + 0xE0)
#define RTC_EN_SUSPEND_REQ      (RTC_CORE_BASE + 0xE4)
#define RTC_PG_REG              (RTC_CORE_BASE + 0xF0)
#define RTC_ST_ON_REASON        (RTC_CORE_BASE + 0xF8)
#define RTC_PWR_DET_SEL         (RTC_CORE_BASE + 0x140)

/* RTC_MACROレジスタ */
#define RTC_MACRO_BASE      (RTC_BASE + 0x26400)
#define RTC_PWR_DET_COMP        (RTC_MACRO_BASE + 0x44)
#define RTC_MACRO_DA_CLEAR_ALL  (RTC_MACRO_BASE + 0x80)
#define RTC_MACRO_DA_SET_ALL    (RTC_MACRO_BASE + 0x84)
#define RTC_MACRO_DA_LATCH_PASS (RTC_MACRO_BASE + 0x88)
#define RTC_MACRO_DA_SOC_READY  (RTC_MACRO_BASE + 0x8C)
#define RTC_MACRO_RG_DEFD       (RTC_MACRO_BASE + 0x94)
#define RTC_MACRO_RG_SET_T      (RTC_MACRO_BASE + 0x98)
#define RTC_MACRO_RO_T          (RTC_MACRO_BASE + 0xA8)

#define RTC_CORE_SRAM_BASE  (RTC_BASE + 0x26800)
#define RTC_CORE_SRAM_SIZE      0x0800 // 2KB
#define RTCSYS_F32KLESS_BASE    0x0502A000

#define RTC_INTERNAL_32K        0
#define RTC_EXTERNAL_32K        1

// SD0/1ベースレジスタからのオフセット
#define CVI_SDHCI_SD_CTRL       (SD0_BASE + 0x200)          // EMMC_CTRL
#define SD_FUNC_EN                  (1 << 0)
#define LATANCY_1T                  (1 << 1)
#define CLK_FREE_EN                 (1 << 2)
#define DISABLE_DATA_CRC_CHK        (1 << 3)
#define SD_RSTN                     (1 << 8)
#define SD_RSTN_OEN                 (1 << 9)
#define TIMER_CLK_SEL_100K          (0 << 16)
#define TIMER_CLK_SEL_32K           (1 << 16)

#define CVI_SDHCI_PHY_TX_RX_DLY     (SD0_BASE + 0x240)      // PHY_TX_RX_DLY
#define CVI_SDHCI_PHY_RX_DLY_SHIFT      16
#define CVI_SDHCI_PHY_RX_DLY_MASK       0x7F0000
#define CVI_SDHCI_PHY_RX_SRC_BIT_1      24
#define CVI_SDHCI_PHY_RX_SRC_BIT_2      25

#define CVI_SDHCI_PHY_DS_DLY        (SD0_BASE + 0x244)      // PHY_DS_DLY
#define CVI_SDHCI_PHY_DLY_STS       (SD0_BASE + 0x248)      // PHY_DLL_STS

#define CVI_SDHCI_PHY_CONFIG        (SD0_BASE + 0x24C)      // PHY_CONFIG
#define REG_TX_BPS_SEL_MASK             0xFFFFFFFE
#define REG_TX_BPS_SEL_CLR_MASK         0x1 // 0x24c  PHY_TX_BPS
#define REG_TX_BPS_SEL_SHIFT            0 // 0x24c  PHY_TX_BPS
#define REG_TX_BPS_SEL_BYPASS           1 // 0x24c PHY_TX_BPS inv

// 電源関係レジスタ

#define SYSCTL_SD_PWRSW_CTRL        (SYSCTL_BASE + 0x1F4)
#define REG_EN_PWRSW                (1 << 0)
#define REG_PWRSW_VSEL_3_3          (0 << 1)
#define REG_PWRSW_VSEL_1_8          (1 << 1)
#define REG_PWRSW_DISC              (1 << 2)
#define REG_PWRSW_AUTO              (1 << 3)

#define IOBLK_SD0_CD                (PINMUX_BASE + 0x900)
#define IOBLK_SD0_PWR_EN            (PINMUX_BASE + 0x904)
#define IOBLK_SD0_CLK               (PINMUX_BASE + 0xA00)
#define IOBLK_SD0_CMD               (PINMUX_BASE + 0xA04)
#define IOBLK_SD0_DAT0              (PINMUX_BASE + 0xA08)
#define IOBLK_SD0_DAT1              (PINMUX_BASE + 0xA0C)
#define IOBLK_SD0_DAT2              (PINMUX_BASE + 0xA10)
#define IOBLK_SD0_DAT3              (PINMUX_BASE + 0xA14)

#define IOBLKC_PU_BIT               (1 << 2)
#define IOBLKC_PD_BIT               (1 << 3)

#define PAD_SD0_CD                  (PINMUX_BASE + 0x18)
#define PAD_SD0_PWR_EN              (PINMUX_BASE + 0x1C)
#define PAD_SD0_CLK                 (PINMUX_BASE + 0x0)
#define PAD_SD0_CMD                 (PINMUX_BASE + 0x4)
#define PAD_SD0_D0                  (PINMUX_BASE + 0x8)
#define PAD_SD0_D1                  (PINMUX_BASE + 0xC)
#define PAD_SD0_D2                  (PINMUX_BASE + 0x10)
#define PAD_SD0_D3                  (PINMUX_BASE + 0x14)

#define PAD_MUX_SD0                 0x0
#define PAD_MUX_GPIO                0x3

#define CLOCK_BYPASS_SELECT         (CLKGEN_BASE + 0x30)
#define DIV_CLK_SD0                 (CLKGEN_BASE + 0x70)
#define SD_MAX_CLOCK                375000000
#define SD_MAX_CLOCK_DIV_VALUE      0x40009
#define CLOCK_BYPASS_SELECT_REGISTER (0x3002030)

#endif /* INC_CV180X_REG_H */
