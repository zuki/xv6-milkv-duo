#ifndef INC_CV181X_REG_H
#define INC_CV181X_REG_H

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
 * GP (General purpose) レジスタ
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


/*
 * Pinmux 定義
 */
#define PINMUX_UART0    0
#define PINMUX_UART1    1
#define PINMUX_UART2    2
#define PINMUX_UART3    3
#define PINMUX_UART3_2  4
#define PINMUX_I2C0     5
#define PINMUX_I2C1     6
#define PINMUX_I2C2     7
#define PINMUX_I2C3     8
#define PINMUX_I2C4     9
#define PINMUX_I2C4_2   10
#define PINMUX_SPI0     11
#define PINMUX_SPI1     12
#define PINMUX_SPI2     13
#define PINMUX_SPI2_2   14
#define PINMUX_SPI3     15
#define PINMUX_SPI3_2   16
#define PINMUX_I2S0     17
#define PINMUX_I2S1     18
#define PINMUX_I2S2     19
#define PINMUX_I2S3     20
#define PINMUX_USBID    21
#define PINMUX_SDIO0    22
#define PINMUX_SDIO1    23
#define PINMUX_ND       24
#define PINMUX_EMMC     25
#define PINMUX_SPI_NOR  26
#define PINMUX_SPI_NAND 27
#define PINMUX_CAM0     28
#define PINMUX_CAM1     29
#define PINMUX_PCM0     30
#define PINMUX_PCM1     31
#define PINMUX_CSI0     32
#define PINMUX_CSI1     33
#define PINMUX_CSI2     34
#define PINMUX_DSI      35
#define PINMUX_VI0      36
#define PINMUX_VO       37
#define PINMUX_RMII1    38
#define PINMUX_EPHY_LED 39
#define PINMUX_I80      40
#define PINMUX_LVDS     41

#define PINMUX_USB_VBUS_DET     (PINMUX_BASE + 0x108)

#define REG_TOP_USB_ECO         (SYSCTL_BASE + 0xB4)
#define BIT_TOP_USB_ECO_RX_FLUSH	0x80
/* rst */
#define REG_TOP_SOFT_RST        0x3000
#define BIT_TOP_SOFT_RST_USB    BIT(11)
#define BIT_TOP_SOFT_RST_SDIO   BIT(14)
#define BIT_TOP_SOFT_RST_NAND   BIT(12)

#define REG_TOP_USB_CTRSTS	(SYSCTL_BASE + 0x38)

#define REG_TOP_CONF_INFO		(SYSCTL_BASE + 0x4)
#define BIT_TOP_CONF_INFO_VBUS		BIT(9)
#define REG_TOP_USB_PHY_CTRL		(SYSCTL_BASE + 0x48)
#define BIT_TOP_USB_PHY_CTRL_EXTVBUS	BIT(0)
#define USB_PHY_ID_OVERRIDE_ENABLE	BIT(6)
#define USB_PHY_ID_VALUE		BIT(7)
#define REG_TOP_DDR_ADDR_MODE		(SYSCTL_BASE + 0x64)

/* irq */
#define IRQ_LEVEL   0
#define IRQ_EDGE    3

/* usb */
#define USB_BASE            0x04340000

/* ethernet phy */
#define ETH_PHY_BASE        0x03009000
#define ETH_PHY_INIT_MASK   0xFFFFFFF9
#define ETH_PHY_SHUTDOWN    BIT(1)
#define ETH_PHY_POWERUP     0xFFFFFFFD
#define ETH_PHY_RESET       0xFFFFFFFB
#define ETH_PHY_RESET_N     BIT(2)
#define ETH_PHY_LED_LOW_ACTIVE  BIT(3)

/* watchdog */
#define CONFIG_DW_WDT_BASE WATCHDOG_BASE
#define CONFIG_DW_WDT_CLOCK_KHZ	25000

#define DW_WDT_CR	0x00
#define DW_WDT_TORR	0x04
#define DW_WDT_CRR	0x0C

#define DW_WDT_CR_EN_OFFSET	0x00
#define DW_WDT_CR_RMOD_OFFSET	0x01
#define DW_WDT_CR_RMOD_VAL	0x00
#define DW_WDT_CRR_RESTART_VAL	0x76

/* SDIO Wifi */
#define WIFI_CHIP_EN_BGA    BIT(18)
#define WIFI_CHIP_EN_QFN    BIT(2)

/* RTC_CTRLレジスタ */
#define RTC_CTRL_BASE       (RTC_BASE + 0x00025000)
#define RTC_FC_COARSE_EN        (RTC_CTRL_BASE + 0x40)
#define RTC_FC_COARSE_CAL       (RTC_CTRL_BASE + 0x44)
#define RTC_FC_FINE_EN          (RTC_CTRL_BASE + 0x48)
#define RTC_FC_FINE_CAL         (RTC_CTRL_BASE + 0x50)
#define RTC_POR_RST_CTRL        (RTC_CTRL_BASE + 0xac)
#define RTC_CTRL0_UNLOCKKEY     0x4
#define RTC_CTRL0               0x8
#define RTC_CTRL0_STATUS0       0xC
#define RTCSYS_RST_CTRL         0x18

/* RTC_COREレジスタ */
#define RTC_CORE_BASE       (RTC_BASE + 0x00026000)
#define RTC_ANA_CALIB           (RTC_CORE_BASE + 0x00)
#define RTC_SEC_PULST_GEN       (RTC_CORE_BASE + 0x04)
#define RTC_SET_SEC_CNTR_VALUE  (RTC_CORE_BASE + 0x10)
#define RTC_SET_SEC_CNTR_TRIG   (RTC_CORE_BASE + 0x14)
#define RTC_SEC_CNTR_VALUE      (RTC_CORE_BASE + 0x18)
#define RTC_INFO0               (RTC_CORE_BASE + 0x1c)
#define RTC_POR_DB_MAGIC_KEY    (RTC_CORE_BASE + 0x68)
#define RTC_EN_PWR_VBAT_DET     (RTC_CORE_BASE + 0xD0)
#define REG_RTC_ST_ON_REASON    (RTC_CORE_BASE + 0xF8)
#define RTC_PWR_DET_SEL         (RTC_CORE_BASE + 0x140)
#define RTC_EN_PWR_WAKEUP       0xBC
#define RTC_EN_SHDN_REQ         0xC0
#define RTC_EN_PWR_CYC_REQ      0xC8
#define RTC_EN_WARM_RST_REQ     0xCC
#define RTC_EN_WDT_RST_REQ      0xE0
#define RTC_EN_SUSPEND_REQ      0xE4
#define RTC_PG_REG              0xF0
#define RTC_ST_ON_REASON        0xF8

/* RTC_MACROレジスタ */
#define RTC_MACRO_BASE          (RTC_BASE + 0x00026400)
#define RTC_PWR_DET_COMP        (RTC_MACRO_BASE + 0x44)

#define RTC_MACRO_DA_SOC_READY  0x8C
#define RTC_MACRO_RO_T          0xA8

#define RTC_CORE_SRAM_BASE      (RTC_BASE + 0x00026800)
#define RTC_CORE_SRAM_SIZE      0x0800 // 2KB


#define RTCSYS_F32KLESS_BASE    (RTC_BASE + 0x0002A000)

#define RTC_INTERNAL_32K    0
#define RTC_EXTERNAL_32K    1

/* eFuse  */
#define EFUSE_BASE (SYSCTL_BASE + 0x00050000)

/* AXI SRAM */
#define AXI_SRAM_BASE 0x0E000000
#define AXI_SRAM_SIZE 0x40

#define EFUSE_SW_INFO_ADDR (AXI_SRAM_BASE)
#define EFUSE_SW_INFO_SIZE 4

#define BOOT_SOURCE_FLAG_ADDR (EFUSE_SW_INFO_ADDR + EFUSE_SW_INFO_SIZE)
#define BOOT_SOURCE_FLAG_SIZE 4
#define MAGIC_NUM_USB_DL 0x4D474E31 // MGN1
#define MAGIC_NUM_SD_DL 0x4D474E32 // MGN2

#define BOOT_LOG_LEN_ADDR (BOOT_SOURCE_FLAG_ADDR + BOOT_SOURCE_FLAG_SIZE) // 0x0E000008
#define BOOT_LOG_LEN_SIZE 4

#define TIME_RECORDS_ADDR (AXI_SRAM_BASE + 0x10) // 0x0E000010

/* from fsbl/plat/cv181x/include/platform_def.h struct _time_records { ... } */
#define TIME_RECORDS_FIELD_UBOOT_START (TIME_RECORDS_ADDR + 0x10)
#define TIME_RECORDS_FIELD_BOOTCMD_START (TIME_RECORDS_ADDR + 0x12)
#define TIME_RECORDS_FIELD_DECOMPRESS_KERNEL_START (TIME_RECORDS_ADDR + 0x14)
#define TIME_RECORDS_FIELD_KERNEL_START (TIME_RECORDS_ADDR + 0x16)

#endif /* INC_CV181X_REG_H */
