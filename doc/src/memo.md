# cycleとtime

- `INTERVAL`
    - CV180x: 250,000
    - QEMU: 1,000,000

```c
printf("s %llu\n", r_time());
msdelay(1000);  // 1秒
printf("e: %llu\n", r_time());
msdelay(100);
printf("s %llu\n", r_time());
usdelay(1000000);  // 1秒
printf("e: %llu\n", r_time());
```

## 実機で実行

```bash
s 88973293
e: 113995998    # e - s = 25,022,705 : 25MHz
s 116523118
e: 141547986    # e - s = 25,024,868 : 25MHz
```

# SDCardを使う

## QEMU、実機ともにエラー

- QEMUのvirtはSDカードをサポートしていない。
- していたとしてもmilk-vとはアドレスが違うだろうからQEMUではSDのテストはできない。

```bash
scause 0xd
sepc=0x802072ae stval=0x43100fc
panic: kerneltrap
```

- これはSD0領域をpageマッピングしていなかったせい

```bash
00000000: 0000 0000 0000 0000 0000 0000 0000 0000
00000010: 0000 0000 0000 0000 0000 0000 0000 0000
00000020: 0000 0000 0000 0000 0000 0000 0000 0000
00000030: 0000 0000 0000 0000 0000 0000 0000 0000
00000040: 0000 0000 0000 0000 0000 0000 0000 0000
00000050: 0000 0000 0000 0000 0000 0000 0000 0000
00000060: 0000 0000 0000 0000 0000 0000 0000 0000
00000070: 0000 0000 0000 0000 0000 0000 0000 0000
00000080: 0000 0000 0000 0000 0000 0000 0000 0000
00000090: 0000 0000 0000 0000 701e 3880 0000 0000
000000a0: 0000 0000 0000 0000 3300 0000 0000 0000
000000b0: 901e 3880 0000 0000 e207 2080 0000 0000
000000c0: 0000 0000 0000 0000 901e 3880 0000 0000
000000d0: 001f 3880 0000 0000 5404 2080 0000 0000
000000e0: 0000 0000 0000 0000 3300 0000 0000 0000
000000f0: 0000 0000 0000 0000 d01e 3880 0000 0000
00000100: 0000 0000 0000 0000 e01e 3880 0000 0000
00000110: f01e 3880 0000 0000 f01e 3880 0000 0000
00000120: 001f 3880 0000 0000 4021 3880 0000 0000
00000130: 101f 3880 0000 0000 0010 0000 0000 0000
00000140: 0600 0000 0000 0070 00f0 f7ff 3f00 0000
00000150: 00f0 df83 0000 0000 0080 e183 c0ff ffff
00000160: 0070 d983 0000 0000 00f0 f7ff 3f00 0000
00000170: 801f 3880 0000 0000 c611 2080 0000 0000
00000180: 1284 ef83 0000 0000 107c 3880 0000 0000
00000190: 1022 3880 0000 0000 a54f faa4 4ffa a44f
000001a0: 107c 3880 0000 0000 00f0 df83 0000 0000
000001b0: 0600 0000 0000 0070 00f0 ffff 3f00 0000
000001c0: 901f 3880 0000 0000 2812 2080 0000 0000
000001d0: e01f 3880 0000 0000 6019 2080 0000 0000
000001e0: 0000 0000 0000 0000 1f07 0000 0000 0000
000001f0: 0000 0000 0000 0000 3825 f383 0000 0000
partition[0]: TYPE: 56, LBA = 0x12280000, #SECS = 0x8020
partition[1]: TYPE: 56, LBA = 0x19600000, #SECS = 0x8020
partition[2]: TYPE: 0, LBA = 0x71f0000, #SECS = 0x0
partition[3]: TYPE: 0, LBA = 0x25380000, #SECS = 0x83f3

# 1024

00000000: 0000 0000 0000 0000 0000 0000 0000 0000
00000010: 0000 0000 0000 0000 0000 0000 0000 0000
00000020: 0000 0000 0000 0000 0000 0000 0000 0000
00000030: 0000 0000 0000 0000 0000 0000 0000 0000
00000040: 0000 0000 0000 0000 0000 0000 0000 0000
00000050: 0000 0000 0000 0000 0000 0000 0000 0000
00000060: 0000 0000 0000 0000 0000 0000 0000 0000
00000070: 0000 0000 0000 0000 0000 0000 0000 0000
00000080: 0000 0000 0000 0000 0000 0000 0000 0000
00000090: 0000 0000 0000 0000 0000 0000 0000 0000
000000a0: 0000 0000 0000 0000 0000 0000 0000 0000
000000b0: 0000 0000 0000 0000 0000 0000 0000 0000
000000c0: 0000 0000 0000 0000 0000 0000 0000 0000
000000d0: 0000 0000 0000 0000 0000 0000 0000 0000
000000e0: 0000 0000 0000 0000 0000 0000 0000 0000
000000f0: 0000 0000 0000 0000 0000 0000 0000 0000
00000100: 0000 0000 0000 0000 0000 0000 0000 0000
00000110: 0000 0000 0000 0000 0000 0000 0000 0000
00000120: 0000 0000 0000 0000 0000 0000 0000 0000
00000130: 0000 0000 0000 0000 0000 0000 0000 0000
00000140: 0000 0000 0000 0000 0000 0000 0000 0000
00000150: 0000 0000 0000 0000 0000 0000 0000 0000
00000160: 0000 0000 0000 0000 0000 0000 0000 0000
00000170: 0000 0000 0000 0000 0000 0000 0000 0000
00000180: 0000 0000 0000 0000 0000 0000 0000 0000
00000190: 0000 0000 0000 0000 0000 0000 0000 0000
000001a0: 0000 0000 0000 0000 0000 0000 0000 0000
000001b0: 0000 0000 0000 0000 0000 0000 0000 0000
000001c0: 0000 0000 0000 0000 0000 0000 0000 0000
000001d0: 0000 0000 0000 0000 0000 0000 0000 0000
000001e0: 0000 0000 0000 0000 0000 0000 0000 0000
000001f0: 0000 0000 0000 0000 0000 0000 0000 0000
00000200: 0000 0000 0000 0000 0000 0000 0000 0000
00000210: 0000 0000 0000 0000 0000 0000 0000 0000
00000220: 0000 0000 0000 0000 0000 0000 0000 0000
00000230: 0000 0000 0000 0000 0000 0000 0000 0000
00000240: 0000 0000 0000 0000 0000 0000 0000 0000
00000250: 0000 0000 0000 0000 0000 0000 0000 0000
00000260: 0000 0000 0000 0000 0000 0000 0000 0000
00000270: 0000 0000 0000 0000 0000 0000 0000 0000
00000280: 0000 0000 0000 0000 701e 3880 0000 0000
00000290: 0000 0000 0000 0000 3300 0000 0000 0000
000002a0: 901e 3880 0000 0000 e207 2080 0000 0000
000002b0: 0000 0000 0000 0000 901e 3880 0000 0000
000002c0: 001f 3880 0000 0000 5404 2080 0000 0000
000002d0: 0000 0000 0000 0000 3300 0000 0000 0000
000002e0: 0000 0000 0000 0000 d01e 3880 0000 0000
000002f0: 0000 0000 0000 0000 e01e 3880 0000 0000
00000300: f01e 3880 0000 0000 f01e 3880 0000 0000
00000310: 001f 3880 0000 0000 4021 3880 0000 0000
00000320: 101f 3880 0000 0000 0010 0000 0000 0000
00000330: 0600 0000 0000 0070 00f0 f7ff 3f00 0000
00000340: 00f0 df83 0000 0000 0080 e183 c0ff ffff
00000350: 0070 d983 0000 0000 00f0 f7ff 3f00 0000
00000360: 801f 3880 0000 0000 c611 2080 0000 0000
00000370: 1284 ef83 0000 0000 107c 3880 0000 0000
00000380: 1022 3880 0000 0000 a54f faa4 4ffa a44f
00000390: 107c 3880 0000 0000 00f0 df83 0000 0000
000003a0: 0600 0000 0000 0070 00f0 ffff 3f00 0000
000003b0: 901f 3880 0000 0000 2812 2080 0000 0000
000003c0: e01f 3880 0000 0000 6019 2080 0000 0000
000003d0: 0000 0000 0000 0000 1f07 0000 0000 0000
000003e0: 0000 0000 0000 0000 3825 f383 0000 0000
000003f0: 68c7 6c83 0000 0000 0090 2080 0000 0000

# デバッグプリントを入れる
ver: 0x00050000
vendor 0x0, SD version 0x5, slot status 0x0
nblock: 0, ull_offset: 0x00000000
control0: 0x0, control1: 0x0, control2: 0x0
warn: no card inserted
fail emmc_card_reset 1
fail emmc_ensure_data_mode
panic: failed emmc_read

# カード有無のチェックを外した
ver: 0x00050000
vendor 0x0, SD version 0x5, slot status 0x0
nblock: 0, ull_offset: 0x00000000
control0: 0x0, control1: 0x0, control2: 0x0
debug: status: 0x0
error: clock did not stabilise within 1 second
fail emmc_card_reset 1
fail emmc_ensure_data_mode
panic: failed emmc_read

# ソフトリセットの解除をした
ver: 0x00050000
vendor 0x0, SD version 0x5, slot status 0x0
nblock: 0, ull_offset: 0x00000000
control0: 0x0, control1: 0x0, control2: 0x800000
debug: status: 0x3f70000    # write portected?
control0: 0xf00, control1: 0xb2003
emmc_issue_command: cmd=0x00000000, arg=0x00000000
EMMC_INTERRUPT: 0x00000000
emmc_issue_command_int is success!
cmd: 0x00000000 (0x00000000), result: success
emmc_issue_command: cmd=0x00000008, arg=0x000001aa
EMMC_INTERRUPT: 0x00000000
warn: error occured whilst waiting for command complete interrupt 1: irpts=0x000
cmd: 0x080a0000 (0x000001aa), result: failure
emmc_issue_command: cmd=0x00000005, arg=0x00000000
EMMC_INTERRUPT: 0x00000000
warn: error occured whilst waiting for command complete interrupt 1: irpts=0x000
cmd: 0x05010000 (0x00000000), result: failure
emmc_issue_command: cmd=0x80000029, arg=0x00000000
EMMC_INTERRUPT: 0x00000000
issuing command ACMD41
warn: error occured whilst waiting for command complete interrupt 1: irpts=0x000
cmd: 0x370a0000 (0x00000000), result: failure
error: failed to inquiry ACMD41
fail emmc_card_reset 1
fail emmc_ensure_data_mode
panic: failed emmc_read
```

## emmc.cの使用をやめ、milk-v版のU-Bootからsdhci.cを導入

### `sdhci_init()`まで正常動作

```bash
[0]sdhci_setup_cfg: sdhci_setup_cfg, caps: 0x3f68c832

[0]sdhci_setup_cfg: sdhci_setup_cfg, caps_1: 0x8006077
[0]sdhci_get_cd: sdhci_get_cd reg = 0x083f70000
[0]sdhci_reset: sdhci_reset-234 MMC0 : ctrl_2 = 0x0080
[0]sdhci_reset: reg_0x24C = 0x00000001, reg_200 = 0x00010302
[0]sdhci_reset: reg_0x240 = 0x01000100, reg_0x248 = 0x00000000
[0]sdhci_get_cd: sdhci_get_cd reg = 0x082070000
[0]sdhci_host_init: set IP clock to 375Mhz
[0]sdhci_host_init: Be sure to switch clock source to PLL
[0]sdhci_host_init: XTAL->PLL reg = 0x0
[0]sdhci_host_init: eMMC/SD CLK is 375000000 in FPGA_ASIC
[0]sdhci_init: sdhci_init ok

init: starting sh
$
```

### `mmc_init()`

```bash
[0]sdhci_setup_cfg: sdhci_setup_cfg, caps: 0x3f68c832
[0]sdhci_setup_cfg: sdhci_setup_cfg, caps_1: 0x8006077
[0]sdhci_get_cd: sdhci_get_cd reg = 0x083f70000
[0]sdhci_reset: sdhci_reset-280 MMC0 : ctrl_2 = 0x0080
[0]sdhci_reset: reg_0x24C = 0x00000001, reg_200 = 0x00010302
[0]sdhci_reset: reg_0x240 = 0x01000100, reg_0x248 = 0x00000000
[0]sdhci_get_cd: sdhci_get_cd reg = 0x082070000
[0]sdhci_host_init: set IP clock to 375Mhz
[0]sdhci_host_init: Be sure to switch clock source to PLL
[0]sdhci_host_init: XTAL->PLL reg = 0x0
[0]sdhci_host_init: eMMC/SD CLK is 375000000 in FPGA_ASIC
[0]sdhci_init: sdhci_init ok
[0]mmc_set_clock: clock is disabled (0Hz)
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]mmc_select_mode: selecting mode MMC legacy (freq : 0 MHz)
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]mmc_set_clock: clock is enabled (400000Hz)
[0]sdhci_set_clock: mmc0 : Set clock 400000, host->max_clk 375000000
[0]sdhci_set_clock: mmc0 : clk div 0x1d5
[0]sdhci_set_clock: mmc0 : 0x2c clk reg 0xd541
[0]sdhci_send_command: sdhci_send_command: Timeout for status update!
[0]mmc_init: mmc_init: -110     # エラー: ETIMEDOUT
init: starting sh
$
```

- このタイムアウトは`get_timer()`関数の実装エラー

```bash
start: 0x0000000000000dfb, end: 0x0000000000000017  # 23msの経過を計測: ok
[0]sdhci_setup_cfg: sdhci_setup_cfg, caps: 0x3f68c832
[0]sdhci_setup_cfg: sdhci_setup_cfg, caps_1: 0x8006077
[0]sdhci_get_cd: sdhci_get_cd reg = 0x083f70000
[0]sdhci_reset: sdhci_reset-280 MMC0 : ctrl_2 = 0x0080
[0]sdhci_reset: reg_0x24C = 0x00000001, reg_200 = 0x00010302
[0]sdhci_reset: reg_0x240 = 0x01000100, reg_0x248 = 0x00000000
[0]sdhci_get_cd: sdhci_get_cd reg = 0x082070000
[0]sdhci_host_init: set IP clock to 375Mhz
[0]sdhci_host_init: Be sure to switch clock source to PLL
[0]sdhci_host_init: XTAL->PLL reg = 0x0
[0]sdhci_host_init: eMMC/SD CLK is 375000000 in FPGA_ASIC
[0]sdhci_init: sdhci_init ok
[0]mmc_set_clock: clock is disabled (0Hz)
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]mmc_select_mode: selecting mode MMC legacy (freq : 0 MHz)
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]mmc_set_clock: clock is enabled (400000Hz)
[0]sdhci_set_clock: mmc0 : Set clock 400000, host->max_clk 375000000
[0]sdhci_set_clock: mmc0 : clk div 0x1d5
[0]sdhci_set_clock: mmc0 : 0x2c clk reg 0xd541
[0]sdhci_send_command: mask: 0x00000001, start: 0x0000000000000e89
[0]sdhci_send_command: stat: 0x00000001, mask: 0x00000001, stat & mask: 0x00000001, (stat & mask) != mase
[0]sdhci_reset: sdhci_reset-280 MMC0 : ctrl_2 = 0x3080
[0]sdhci_reset: reg_0x24C = 0x00000001, reg_200 = 0x00010002
[0]sdhci_reset: reg_0x240 = 0x01000100, reg_0x248 = 0x00000000
[0]sdhci_reset: sdhci_reset-280 MMC0 : ctrl_2 = 0x3080
[0]sdhci_reset: reg_0x24C = 0x00000001, reg_200 = 0x00010002
[0]sdhci_reset: reg_0x240 = 0x01000100, reg_0x248 = 0x00000000
[0]mmc_init: mmc_init: -70
init: starting sh
```

## チューニングをやめた

```bash
[0]sdhci_setup_cfg: sdhci_setup_cfg, caps: 0x3f68c832
[0]sdhci_setup_cfg: sdhci_setup_cfg, caps_1: 0x8006077
mmc_config: name: cv180x_sdhci caps: 0x2000000e volt:
            fmin: 0x00061a80 fmax: 0x0bebc200 bmax: 0x0000ffff type: 0x00
[0]sdhci_get_cd: SDHCI_PRESENT_STATE: 0x03f70000
[0]sdhci_get_cd: sd present
[0]sdhci_host_init: 1: call sdhci_reset
[0]sdhci_reset: MMC0 : ctrl_2 = 0x0080
[0]sdhci_reset: PHY_CONFIG: 0x00000001, VENDOR_OFFSET: 0x00010302
[0]sdhci_reset: PHY_TX_RX_DLY: 0x01000100, PHY_DLY_STS: 0x00000000
[0]sdhci_get_cd: SDHCI_PRESENT_STATE: 0x02070000
[0]sdhci_get_cd: sd present
[0]sdhci_host_init: 2: get_cd: 1
[0]sdhci_host_init: 3: set_power
[0]sdhci_set_power: set 3.3
[0]sdhci_host_init: 4: int_enable: 0x27f003b8x
[0]sdhci_host_init: 5: signal_enable: 0x08x
[0]sdhci_host_init: 6: set max_clock_div_value: 0x400098x
[0]sdhci_host_init: 7: switch clock source to PLL: 0x00000000
[0]sdhci_host_init: eMMC/SD CLK is 375000000 in FPGA_ASIC
[0]sdhci_host_init: 8: host_cntl2: 0x3080
[0]sdhci_init: sdhci_init ok
[0]mmc_get_op_cond: uhs support: false
[0]mmc_set_clock: clock is disabled (0Hz)
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]sdhci_set_clock: state: 0x03f70000
[0]sdhci_set_ios: org host_contorl: 0x00000000
[0]sdhci_set_ios: set host_contorl: 0x00000000
[0]sdhci_set_voltage: switch to 3.3v
[0]sdhci_set_uhs_timing: set host_ctrl2: 0x00003080
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]sdhci_set_clock: state: 0x03f70000
[0]sdhci_set_ios: org host_contorl: 0x00000000
[0]sdhci_set_ios: set host_contorl: 0x00000000
[0]mmc_select_mode: selecting mode MMC legacy (freq : 0 MHz)
[0]sdhci_set_clock: mmc0 : Set clock 0, host->max_clk 375000000
[0]sdhci_set_clock: state: 0x03f70000
[0]sdhci_set_ios: org host_contorl: 0x00000000
[0]sdhci_set_ios: set host_contorl: 0x00000000
[0]mmc_set_bus_width: mmc bus width: 1
[0]mmc_set_clock: clock is enabled (400000Hz)
[0]sdhci_set_clock: mmc0 : Set clock 400000, host->max_clk 375000000
[0]sdhci_set_clock: state: 0x03f70000
[0]sdhci_set_clock: mmc0 : clk div 0x1d5
[0]sdhci_set_clock: mmc0 : 0x2c clk reg 0xd541
[0]sdhci_set_ios: org host_contorl: 0x00000000
[0]sdhci_set_ios: set host_contorl: 0x00000000
[0]mmc_go_idle: idle state
[0]mmc_send_if_cond: sd veriosn 2
[0]sd_send_op_cond: high_capacity: true
[0]mmc_startup: csd[0-3]: 400e0032 5b590000 e8f37f80 0a400000
[0]mmc_select_mode: selecting mode MMC legacy (freq : 25 MHz)
mmc_dump_capabilities: sd card: widths [4, 1] modes [MMC legacy, SD High Speed (50MHz), UHS SDR12 (25MHz), UHS SDR25 (50MH])
mmc_dump_capabilities: host: widths [4, 1] modes [MMC legacy, MMC High Speed (26MHz), SD High Speed (50MHz), MMC High Spee]
[0]sd_select_mode_and_width: trying mode SD High Speed (50MHz) width 4 (at 50 MHz)
[0]sd_select_bus_width: set sd bus width: 4
[0]sdhci_set_clock: mmc0 : Set clock 400000, host->max_clk 375000000
[0]sdhci_set_clock: state: 0x03f70000
[0]sdhci_set_clock: mmc0 : clk div 0x1d5
[0]sdhci_set_clock: mmc0 : 0x2c clk reg 0xd541
[0]sdhci_set_ios: org host_contorl: 0x00000000
[0]sdhci_set_ios: set host_contorl: 0x00000002
[0]mmc_set_bus_width: mmc bus width: 4
[0]sd_set_card_speed: speed: 0x00000001, status[4]: 0x00000000
[0]mmc_select_mode: selecting mode SD High Speed (50MHz) (freq : 50 MHz)
[0]mmc_set_clock: clock is enabled (50000000Hz)
[0]sdhci_set_clock: mmc0 : Set clock 50000000, host->max_clk 375000000
[0]sdhci_set_clock: state: 0x03f70000
[0]sdhci_set_clock: mmc0 : clk div 0x4
[0]sdhci_set_clock: mmc0 : 0x2c clk reg 0x401
[0]sdhci_set_ios: org host_contorl: 0x00000002
[0]sdhci_set_ios: set host_contorl: 0x00000006
[0]mmc_dump_mmc: offset: 0
[0]mmc_dump_mmc: cfg
[0]mmc_dump_mmc:   name     : cv180x_sdhci
[0]mmc_dump_mmc:   host_caps: 0x2000000e    # BIt: 29, 3, 2, 1
[0]mmc_dump_mmc:   voltages : 3539072   # 0011_0110_0000_1000_0000
[0]mmc_dump_mmc:   f_min    : 400000
[0]mmc_dump_mmc:   f_max    : 200000000
[0]mmc_dump_mmc:   b_max    : 65535
[0]mmc_dump_mmc:   part_type: 0
[0]mmc_dump_mmc: version       : 0x80030000
[0]mmc_dump_mmc: priv          : 0x80399188
[0]mmc_dump_mmc: has_init      : 0
[0]mmc_dump_mmc: high_capacity : 1
[0]mmc_dump_mmc: clk_disable   : false
[0]mmc_dump_mmc: bus_width     : 4
[0]mmc_dump_mmc: clock         : 50000000
[0]mmc_dump_mmc: saved_clock   : 0
[0]mmc_dump_mmc: signal_voltage: 4
[0]mmc_dump_mmc: card_caps     : 0x30000065
[0]mmc_dump_mmc: host_caps     : 0x3000000f
[0]mmc_dump_mmc: ocr           : 0xc0ff8000
[0]mmc_dump_mmc: dsr           : 0x00000000
[0]mmc_dump_mmc: dsr_imp       : 0x00000000
[0]mmc_dump_mmc: scr[0-1]      : 0x02858082 0x02858082
[0]mmc_dump_mmc: csd[0-3]      : 0x400e0032 0x5b590000 0xe8f37f80 0x0a400000
[0]mmc_dump_mmc: cid[0-3]      : 0x27504853 0x44333247 0x60785659 0x8c016800
[0]mmc_dump_mmc: rca           : 0x00000001
[0]mmc_dump_mmc: part_support  : 0
[0]mmc_dump_mmc: part_attr     : 0
[0]mmc_dump_mmc: wr_rel_set    : 0
[0]mmc_dump_mmc: part_config   : 255
[0]mmc_dump_mmc: gen_cmd6_time : 0
[0]mmc_dump_mmc: part_switch_time: 0
[0]mmc_dump_mmc: tran_speed    : 50000000
[0]mmc_dump_mmc: legacy_speed: 25000000
[0]mmc_dump_mmc: read_bl_len   : 512
[0]mmc_dump_mmc: write_bl_len  : 512
[0]mmc_dump_mmc: erase_grp_size: 1
[0]mmc_dump_mmc: ssr
[0]mmc_dump_mmc:   au           : 8192
[0]mmc_dump_mmc:   erase_timeout: 250
[0]mmc_dump_mmc:   erase_offset : 2000
[0]mmc_dump_mmc: capacity      : 0lld
[0]mmc_dump_mmc: capacity_user : 1201668096lld
[0]mmc_dump_mmc: capacity_boot : 0lld
[0]mmc_dump_mmc: capacity_rpmb : 0lld
[0]mmc_dump_mmc: capacity_gp[0-3]: 0lld  0lld 0lld 0lld
[0]mmc_dump_mmc: capacity_boot : 0lld
[0]mmc_dump_mmc: op_cond_pending : 0
[0]mmc_dump_mmc: init_in_progress: 0
[0]mmc_dump_mmc: preinit       : 0
[0]mmc_dump_mmc: ddr_mode      : 0
[0]mmc_dump_mmc: ext_csd       : 0
[0]mmc_dump_mmc: cardtype      : 0
[0]mmc_dump_mmc: current_voltage: 0
[0]mmc_dump_mmc: selected_mode : 2
[0]mmc_dump_mmc: best_mode     : 2
[0]mmc_dump_mmc: quirks        : 0x00000007
[0]mmc_dump_mmc: hs400_tuning  : 0
[0]mmc_dump_mmc: user_speed_mode: 0
init: starting sh
$
```

## 再度、emmcに戻す

```bash
[0]emmc_set_host_data: caps1: 0x0011_1111_0110_1000_1100_1000_0011_0010, caps2: 0x0000_1000_0000_0000_0110_0000_0111_0111, version: 0x0000_0001
[0]emmc_dump_host: struct emmc
[0]emmc_dump_host:  caps    : 0010_0000_0000_0000_0000_0000_0000_1110
[0]emmc_dump_host:  version : 5
[0]emmc_dump_host:  max_clk : 375000000
[0]emmc_dump_host:  f_min   : 400000
[0]emmc_dump_host:  f_max   : 200000000
[0]emmc_dump_host:  b_max   : 65535
[0]emmc_dump_host:  voltages: 0000_0000_0011_0110_0000_0000_1000_0000
[0]emmc_dump_host:  vol_18v : false
[0]emmc_power_on_off: mode 02, vdd 0015

[0]emmc_power_noreg: self->pwr: 0x00, pwr: 0x0e
[0]emmc_power_noreg: SD_BUS_POWER_CNTL: 0x0f
[0]emmc_voltage_restore: SYSCTL_SD_PWRSW_CTRL: 0x00000009
[0]emmc_voltage_restore: CVI_SDHCI_SD_CTRL: 0x00000302, CVI_SDHCI_PHY_TX_RX_DLY: 0x01000100, CVI_SDHCI_PHY_CONFIG: 0x00000001
[0]emmc_setup_pad: PAD_SD0 PWR_EN: 0, CLK: 0, CMD: 0, D0: 0, D1: 0, D2: 0, D3: 0, CD: 0
[0]emmc_setup_io: IOBLK_SD0 PWR_EN: 64, CLK: 64, CMD: 68, D0: 68, D1: 68, D2: 68, D3: 68, CD: 64
[0]emmc_card_init: set IP clock to 375Mhz
[0]emmc_card_init: Be sure to switch clock source to PLL
[0]emmc_card_init: XTAL->PLL reg = 0x0
[0]emmc_card_init: SD CLK is 375000000 in FPGA_ASIC
[0]emmc_card_init: 8: host_cntl2: 0x30800000
[0]emmc_card_init: hci_ver: 5
[0]emmc_read: nblock: 0x0, ull_offset: 0x0
[0]emmc_card_reset: control0: 0x00000000, control1: 0x00000000, control2: 0x00800000
[0]emmc_card_reset: status: 0x03f70000
[0]emmc_switch_clock_rate: Set clock 400000, self->max_clk 375000000
[0]emmc_switch_clock_rate: clk div 0x1d5

[0]emmc_switch_clock_rate: SD_CONTROL1: 0xd541

[0]emmc_issue_command: emmc_issue_command: cmd=0, arg=0x00000000
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 0 (0x00000000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=8, arg=0x000001aa
[0]emmc_issue_command_int: error waiting for command complete interrupt 1: irpts=0b0000_0000_0000_0001_1000_0000_0000_0000
[0]emmc_issue_command: cmd: 8 (0x000001aa), result: failure
[0]emmc_issue_command: emmc_issue_command: cmd=5, arg=0x00000000
[0]emmc_issue_command_int: error waiting for command complete interrupt 1: irpts=0b0000_0000_0000_0001_1000_0000_0000_0000
[0]emmc_issue_command: cmd: 5 (0x00000000), result: failure
[0]emmc_issue_command: emmc_issue_command: cmd=41, arg=0x00000000
[0]emmc_issue_command: issuing command ACMD41
[0]emmc_issue_command_int: error waiting for command complete interrupt 1: irpts=0b0000_0000_0000_0001_1000_0000_0000_0000
[0]emmc_issue_command: cmd: 55 (0x00000000), result: failure
[0]emmc_card_reset: failed to inquiry ACMD41
[0]emmc_ensure_data_mode: fail emmc_card_reset 1
[0]emmc_do_read: fail emmc_ensure_data_mode
panic: failed emmc_read

```

## 読み込みに成功

```bash
C.SCS/0/0.WD.URPL.SDI/25000000/6000000.BS/SD.PS.SD/0x0/0x1000/0x1000/0.PE.BS.SD/0x1000/0xba00/0xba00/0.BE.J.
FSBL Jb28g9:gf2df47913:2024-02-29T16:35:38+08:00
st_on_reason=d0000
st_off_reason=0
P2S/0x1000/0x3bc0da00.
SD/0xca00/0x1000/0x1000/0.P2E.
DPS/0xda00/0x2000.
SD/0xda00/0x2000/0x2000/0.DPE.
DDR init.
ddr_param[0]=0x78075562.
pkg_type=3
D3_1_4
DDR2-512M-QFN68
Data rate=1333.
DDR BIST PASS
PLLS.
PLLE.
C2S/0xfa00/0x83f40000/0x3600.
[R2TE:. 0/0x3600/0x3600/0.RSC.
.M.S4/204x317300]0P0r/e0 xs8y0s0t0e0m0 0i0n/i0tx 1dbo0n0e0
R
T: [1.430690]CVIRTOS Build Date:Feb 29 2024  (Time :16:35:37)
RT: [1.436693]Post system init done
RT: [1.440009]create cvi task
RT: [1.442835][cvi_spinlock_init] succeess
RT: [1.446728]prvCmdQuRunTask run
SD/0x13000/0x1b000/0x1b000/0.ME.
L2/0x2e000.
SD/0x2e000/0x200/0x200/0.L2/0x414d3342/0xcafebfd8/0x80200000/0x23000/0x23000
COMP/1.
SD/0x2e000/0x23000/0x23000/0.DCP/0x80200020/0x1000000/0x81500020/0x23000/1.
DCP/0x4a307/0.
Loader_2nd loaded.
Use internal 32k
Jump to monitor at 0x80000000.
OPENSBI: next_addr=0x80200020 arg1=0x80080000
OpenSBI v0.9
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name             : Milk-V Duo
Platform Features         : mfdeleg
Platform HART Count       : 1
Platform IPI Device       : clint
Platform Timer Device     : clint
Platform Console Device   : uart8250
Platform HSM Device       : ---
Platform SysReset Device  : ---
Firmware Base             : 0x80000000
Firmware Size             : 132 KB
Runtime SBI Version       : 0.3

Domain0 Name              : root
Domain0 Boot HART         : 0
Domain0 HARTs             : 0*
Domain0 Region00          : 0x0000000074000000-0x000000007400ffff (I)
Domain0 Region01          : 0x0000000080000000-0x000000008003ffff ()
Domain0 Region02          : 0x0000000000000000-0xffffffffffffffff (R,W,X)
Domain0 Next Address      : 0x0000000080200020
Domain0 Next Arg1         : 0x0000000080080000
Domain0 Next Mode         : S-mode
Domain0 SysReset          : yes

Boot HART ID              : 0
Boot HART Domain          : root
Boot HART ISA             : rv64imafdcvsux
Boot HART Features        : scounteren,mcounteren,time
Boot HART PMP Count       : 16
Boot HART PMP Granularity : 4096
Boot HART PMP Address Bits: 38
Boot HART MHPM Count      : 8
Boot HART MHPM Count      : 8
Boot HART MIDELEG         : 0x0000000000000222
Boot HART MEDELEG         : 0x000000000000b109


U-Boot 2021.10 (Feb 29 2024 - 16:53:07 +0800)cvitek_cv180x

DRAM:  63.3 MiB
gd->relocaddr=0x83ef8000. offset=0x3cf8000
MMC:   cv-sd@4310000: 0
Loading Environment from <NULL>... OK
In:    serial
Out:   serial
Err:   serial
Net:
Warning: ethernet@4070000 (eth0) using random MAC address - ea:1b:df:4d:e6:4d
eth0: ethernet@4070000
Hit any key to stop autoboot:  0
Boot from SD ...
switch to partitions #0, OK
mmc0 is current device
1603021 bytes read in 73 ms (20.9 MiB/s)
## Loading kernel from FIT Image at 81400000 ...
   Using 'config-cv1800b_milkv_duo_sd' configuration
   Trying 'kernel-1' kernel subimage
   Verifying Hash Integrity ... crc32+ OK
## Loading fdt from FIT Image at 81400000 ...
   Using 'config-cv1800b_milkv_duo_sd' configuration
   Trying 'fdt-1' fdt subimage
   Verifying Hash Integrity ... sha256+ OK
   Booting using the fdt blob at 0x8158615c
   Loading Kernel Image
   Decompressing 1597440 bytes used 2ms
   Loading Device Tree to 00000000836ac000, end 00000000836aff73 ... OK

Starting kernel ...


xv6 kernel is booting

SBI specification v0.3 detected
SBI TIME extension detected
SBI IPI extension detected
SBI RFNC extension detected
SBI HSM extension detected

[0]mmc_dump_caps1: SD_CAPABILITIES_1
[0]mmc_dump_caps1:  tout_clk_freq    : 50
[0]mmc_dump_caps1:  tout_clk_unit    : 0
[0]mmc_dump_caps1:  base_clk_freq    : 200
[0]mmc_dump_caps1:  max_blk_len      : 0
[0]mmc_dump_caps1:  embeeded_8bit    : 0
[0]mmc_dump_caps1:  adaa2_support    : 1
[0]mmc_dump_caps1:  hs_support       : 1
[0]mmc_dump_caps1:  sdma_support     : 1
[0]mmc_dump_caps1:  sus_res_support  : 0
[0]mmc_dump_caps1:  v33_support      : 1
[0]mmc_dump_caps1:  v30_support      : 1
[0]mmc_dump_caps1:  v18_support      : 1
[0]mmc_dump_caps1:  bus64_support    : 1
[0]mmc_dump_caps1:  async_int_support: 1
[0]mmc_dump_caps1:  slot_type        : 0
[0]mmc_dump_caps2: SD_CAPABILITIES_2
[0]mmc_dump_caps2:  sdr50_support : 1
[0]mmc_dump_caps2:  sdr104_support: 1
[0]mmc_dump_caps2:  ddr50_support : 1
[0]mmc_dump_caps2:  drv_a_support : 1
[0]mmc_dump_caps2:  drv_a_support : 1
[0]mmc_dump_caps2:  drv_a_support : 1
[0]mmc_dump_caps2:  retune_timer  : 0x0
[0]mmc_dump_caps2:  tune_sdr50    : 1
[0]mmc_dump_caps2:  retune_mode   : 1
[0]mmc_dump_caps2:  clk_muliplier : 0x08
[0]mmc_dump_version: SD_HOST_VERSION
[0]mmc_dump_version:  spec  : 5
[0]mmc_dump_version:  vendor: 0x00
[0]emmc_dump_host: struct sdhci_host
[0]emmc_dump_host:  caps    : 0010_0000_0000_0000_0000_0000_0000_1110
[0]emmc_dump_host:  version : 5
[0]emmc_dump_host:  max_clk : 375000000
[0]emmc_dump_host:  f_min   : 400000
[0]emmc_dump_host:  f_max   : 200000000
[0]emmc_dump_host:  b_max   : 65535
[0]emmc_dump_host:  voltages: 0000_0000_0011_0110_0000_0000_1000_0000
[0]emmc_dump_host:  vol_18v : false
[0]emmc_power_on_off: mode: 2, vdd: 0x0015
[0]emmc_power_noreg: self->pwr: 0x00, pwr: 0x0e
[0]emmc_power_noreg: SD_BUS_POWER_CNTL: 0x0f
[0]emmc_voltage_restore: SYSCTL_SD_PWRSW_CTRL: 0x00000009
[0]emmc_voltage_restore: CVI_SDHCI_SD_CTRL      : 0x00000302
[0]emmc_voltage_restore: CVI_SDHCI_PHY_TX_RX_DLY: 0x01000100,
[0]emmc_voltage_restore: CVI_SDHCI_PHY_CONFIG   : 0x00000001
[0]emmc_setup_pad: PAD_SD0
[0]emmc_setup_pad:  PWR_EN: 0, CLK:  0, CMD:  0, D0:  0, D1:  0, D2:  0, D3:  0, CD:  0
[0]emmc_setup_io: IOBLK_SD0
[0]emmc_setup_io:  PWR_EN: 136, CLK: 136, CMD: 4, D0: 4, D1: 4, D2: 4, D3: 4, CD: 4             // 修正
[0]emmc_card_init: PLL Setting
[0]emmc_card_init:  Clock: 375000000, DIV_CLK_SD0: 0x00000000, CLOCK_BYPASS_SELECT: 0x00000000
[0]emmc_host_ctl2: HOST_CTL2
[0]emmc_host_ctl2:  uhs_mode_sel     : 0
[0]emmc_host_ctl2:  en_18_sig        : 0
[0]emmc_host_ctl2:  drv_sel          : 0
[0]emmc_host_ctl2:  execute_tune     : 0
[0]emmc_host_ctl2:  sample_clk_sel   : 1
[0]emmc_host_ctl2:  async_int_en     : 0
[0]emmc_host_ctl2:  preset_val_enable: 0
[0]emmc_card_init: CVI_SDHCI_SD_CTRL      : 0x00000002
[0]emmc_card_init: CVI_SDHCI_PHY_TX_RX_DLY: 0x01000100
[0]emmc_card_init: CVI_SDHCI_PHY_CONFIG   : 0x00000001
[0]emmc_card_init: hci_ver: 5
[0]emmc_read: nblock: 0x0, ull_offset: 0x0
[0]emmc_dump_host_ctl1: HOST_CTL1
[0]emmc_dump_host_ctl1:  lec_ctl              : 0
[0]emmc_dump_host_ctl1:  dat_xfer_width       : 0
[0]emmc_dump_host_ctl1:  hs_enable            : 0
[0]emmc_dump_host_ctl1:  dma_sel              : 0
[0]emmc_dump_host_ctl1:  ext_dat_width        : 0
[0]emmc_dump_host_ctl1:  card_det_test        : 0
[0]emmc_dump_host_ctl1:  card_det_sel         : 0
[0]emmc_dump_host_ctl1:  sd_bus_pwr           : 1
[0]emmc_dump_host_ctl1:  sd_bus_vol_sel       : b111
[0]emmc_dump_host_ctl1:  stop_bg_req          : 0
[0]emmc_dump_host_ctl1:  continue_req         : 0
[0]emmc_dump_host_ctl1:  read_wait            : 0
[0]emmc_dump_host_ctl1:  int_bg               : 0
[0]emmc_dump_host_ctl1:  wakeup_on_card_int   : 0
[0]emmc_dump_host_ctl1:  wakeup_on_card_insert: 0
[0]emmc_dump_host_ctl1:  wakeup_on_card_ready : 0
[0]emmc_dump_clk_ctl: CLK_CTL_SWRST
[0]emmc_dump_clk_ctl:  int_clk_en    : 0
[0]emmc_dump_clk_ctl:  int_clk_stable: 0
[0]emmc_dump_clk_ctl:  sd_clk_em     : 0
[0]emmc_dump_clk_ctl:  pll_en        : 0
[0]emmc_dump_clk_ctl:  up_freq_sel   : 0
[0]emmc_dump_clk_ctl:  freq_sel      : 0x00
[0]emmc_dump_clk_ctl:  tout_cnt      : 0x0
[0]emmc_dump_clk_ctl:  sw_rst_all    : 0
[0]emmc_dump_clk_ctl:  sw_rst_cmd    : 0
[0]emmc_dump_clk_ctl:  sw_rst_dat    : 0
[0]emmc_card_reset: SD_PRESENT_STS: 0x03f70000
[0]emmc_switch_clock_rate: Set clock 400000, self->max_clk 375000000
[0]emmc_switch_clock_rate: clk div 0x1d5

[0]emmc_switch_clock_rate: SD_CONTROL1: 0xd541

[0]emmc_issue_command: emmc_issue_command: cmd=0, arg=0x00000000
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 0 (0x00000000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=8, arg=0x000001aa
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 8 (0x000001aa), result: success
[0]emmc_dump_mmc: struct mmc
[0]emmc_dump_mmc:  ull_offset            : 0
[0]emmc_dump_mmc:  hci_ver               : 5
[0]emmc_dump_mmc:  device_id             : [0, 0, 0, 0]
[0]emmc_dump_mmc:  card_supports_sdhc    : 0
[0]emmc_dump_mmc:  card_supports_hs      : 0
[0]emmc_dump_mmc:  card_supports_18v     : 0
[0]emmc_dump_mmc:  card_ocr              : 0x00000000
[0]emmc_dump_mmc:  card_rca              : 0x00000000
[0]emmc_dump_mmc:  last_interrupt        : 0x00000000
[0]emmc_dump_mmc:  last_error            :0
[0]emmc_dump_mmc:  failed_voltage_switch : 0
[0]emmc_dump_mmc:  last_cmd_reg          : 0x080a0000
[0]emmc_dump_mmc:  last_cmd              : 0x00000008
[0]emmc_dump_mmc:  last_cmd_success      : 1
[0]emmc_dump_mmc:  last_r0               : 0x000001aa
[0]emmc_dump_mmc:  last_r1               : 0x00000000
[0]emmc_dump_mmc:  last_r2               : 0x00000000
[0]emmc_dump_mmc:  last_r3               : 0x00000000
[0]emmc_dump_mmc:  blocks_to_transfer    : 0
[0]emmc_dump_mmc:  block_size            : 0
[0]emmc_dump_mmc:  card_removal          : 0
[0]emmc_dump_mmc:  pwr                   : 14
[0]emmc_dump_host: struct sdhci_host
[0]emmc_dump_host:  caps    : 0010_0000_0000_0000_0000_0000_0000_1110
[0]emmc_dump_host:  version : 5
[0]emmc_dump_host:  max_clk : 375000000
[0]emmc_dump_host:  f_min   : 400000
[0]emmc_dump_host:  f_max   : 200000000
[0]emmc_dump_host:  b_max   : 65535
[0]emmc_dump_host:  voltages: 0000_0000_0011_0110_0000_0000_1000_0000
[0]emmc_dump_host:  vol_18v : false
[0]emmc_issue_command: emmc_issue_command: cmd=5, arg=0x00000000
[0]emmc_issue_command_int: error waiting for command complete interrupt 1: irpts=0b0000_0000_0000_0001_1000_0000_0000_0000
[0]emmc_issue_command: cmd: 5 (0x00000000), result: failure
[0]emmc_issue_command: emmc_issue_command: cmd=41, arg=0x00000000
[0]emmc_issue_command: issuing command ACMD41
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 55 (0x00000000), result: success
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 41 (0x00000000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=41, arg=0x40ff8000
[0]emmc_issue_command: issuing command ACMD41
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 55 (0x00000000), result: success
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 41 (0x40ff8000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=41, arg=0x40ff8000
[0]emmc_issue_command: issuing command ACMD41
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 55 (0x00000000), result: success
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 41 (0x40ff8000), result: success
[0]emmc_card_reset: OCR: 0xff80, 1.8v support: 0, SDHC support: 1
[0]emmc_switch_clock_rate: Set clock 25000000, self->max_clk 375000000
[0]emmc_switch_clock_rate: clk div 0x8

[0]emmc_switch_clock_rate: SD_CONTROL1: 0x801

[0]emmc_issue_command: emmc_issue_command: cmd=2, arg=0x00000000
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 2 (0x00000000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=3, arg=0x00000000
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 3 (0x00000000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=7, arg=0x00010000
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 7 (0x00010000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=51, arg=0x00000000
[0]emmc_issue_command: issuing command ACMD51
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 55 (0x00010000), result: success
[0]emmc_issue_command_int: block transfer complete
[0]emmc_issue_command_int: emmc_issue_command_int: 268435456 is success!
[0]emmc_issue_command: cmd: 51 (0x00000000), result: success
[0]emmc_card_reset: debug: SCR: version 3.0x, bus_widths 0x5
[0]emmc_issue_command: emmc_issue_command: cmd=6, arg=0x00fffff0
[0]emmc_issue_command_int: block transfer complete
[0]emmc_issue_command_int: emmc_issue_command_int: 268435456 is success!
[0]emmc_issue_command: cmd: 6 (0x00fffff0), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=6, arg=0x80fffff1
[0]emmc_issue_command_int: block transfer complete
[0]emmc_issue_command_int: emmc_issue_command_int: 268435456 is success!
[0]emmc_issue_command: cmd: 6 (0x80fffff1), result: success
[0]emmc_switch_clock_rate: Set clock 50000000, self->max_clk 375000000
[0]emmc_switch_clock_rate: clk div 0x4

[0]emmc_switch_clock_rate: SD_CONTROL1: 0x401

[0]emmc_issue_command: emmc_issue_command: cmd=6, arg=0x00000002
[0]emmc_issue_command: issuing command ACMD6
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 55 (0x00010000), result: success
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 6 (0x00000002), result: success
[0]emmc_card_reset: info: found valid version 3.0x SD card
[0]emmc_issue_command: emmc_issue_command: cmd=13, arg=0x00010000
[0]emmc_issue_command_int: emmc_issue_command_int: 0 is success!
[0]emmc_issue_command: cmd: 13 (0x00010000), result: success
[0]emmc_issue_command: emmc_issue_command: cmd=18, arg=0x00000000
[0]emmc_issue_command_int: multi-block transfer
[0]emmc_issue_command_int: block transfer complete
[0]emmc_issue_command_int: emmc_issue_command_int: 905969664 is success!
[0]emmc_issue_command: cmd: 18 (0x00000000), result: success

00000000: 0000 0000 0000 0000 0000 0000 0000 0000
00000010: 0000 0000 0000 0000 0000 0000 0000 0000
00000020: 0000 0000 0000 0000 0000 0000 0000 0000
00000030: 0000 0000 0000 0000 0000 0000 0000 0000
00000040: 0000 0000 0000 0000 0000 0000 0000 0000
00000050: 0000 0000 0000 0000 0000 0000 0000 0000
00000060: 0000 0000 0000 0000 0000 0000 0000 0000
00000070: 0000 0000 0000 0000 0000 0000 0000 0000
00000080: 0000 0000 0000 0000 0000 0000 0000 0000
00000090: 0000 0000 0000 0000 0000 0000 0000 0000
000000a0: 0000 0000 0000 0000 0000 0000 0000 0000
000000b0: 0000 0000 0000 0000 0000 0000 0000 0000
000000c0: 0000 0000 0000 0000 0000 0000 0000 0000
000000d0: 0000 0000 0000 0000 0000 0000 0000 0000
000000e0: 0000 0000 0000 0000 0000 0000 0000 0000
000000f0: 0000 0000 0000 0000 0000 0000 0000 0000
00000100: 0000 0000 0000 0000 0000 0000 0000 0000
00000110: 0000 0000 0000 0000 0000 0000 0000 0000
00000120: 0000 0000 0000 0000 0000 0000 0000 0000
00000130: 0000 0000 0000 0000 0000 0000 0000 0000
00000140: 0000 0000 0000 0000 0000 0000 0000 0000
00000150: 0000 0000 0000 0000 0000 0000 0000 0000
00000160: 0000 0000 0000 0000 0000 0000 0000 0000
00000170: 0000 0000 0000 0000 0000 0000 0000 0000
00000180: 0000 0000 0000 0000 0000 0000 0000 0000
00000190: 0000 0000 0000 0000 0000 0000 0000 0000
000001a0: 0000 0000 0000 0000 0000 0000 0000 0000
000001b0: 0000 0000 0000 0000 0000 0000 0000 8000
000001c0: 0200 0c28 2108 0100 0000 0000 0200 0000
000001d0: 0000 0000 0000 0000 0000 0000 0000 0000
000001e0: 0000 0000 0000 0000 0000 0000 0000 0000
000001f0: 0000 0000 0000 0000 0000 0000 0000 55aa
00000200: eb3c 906d 6b66 732e 6661 7400 0204 0400
00000210: 0200 0200 00f8 8000 2000 0800 0000 0000
00000220: 0000 0200 8000 29dc 7e85 924e 4f20 4e41
00000230: 4d45 2020 2020 4641 5431 3620 2020 0e1f
00000240: be5b 7cac 22c0 740b 56b4 0ebb 0700 cd10
00000250: 5eeb f032 e4cd 16cd 19eb fe54 6869 7320
00000260: 6973 206e 6f74 2061 2062 6f6f 7461 626c
00000270: 6520 6469 736b 2e20 2050 6c65 6173 6520
00000280: 696e 7365 7274 2061 2062 6f6f 7461 626c
00000290: 6520 666c 6f70 7079 2061 6e64 0d0a 7072
000002a0: 6573 7320 616e 7920 6b65 7920 746f 2074
000002b0: 7279 2061 6761 696e 202e 2e2e 200d 0a00
000002c0: 0000 0000 0000 0000 0000 0000 0000 0000
000002d0: 0000 0000 0000 0000 0000 0000 0000 0000
000002e0: 0000 0000 0000 0000 0000 0000 0000 0000
000002f0: 0000 0000 0000 0000 0000 0000 0000 0000
00000300: 0000 0000 0000 0000 0000 0000 0000 0000
00000310: 0000 0000 0000 0000 0000 0000 0000 0000
00000320: 0000 0000 0000 0000 0000 0000 0000 0000
00000330: 0000 0000 0000 0000 0000 0000 0000 0000
00000340: 0000 0000 0000 0000 0000 0000 0000 0000
00000350: 0000 0000 0000 0000 0000 0000 0000 0000
00000360: 0000 0000 0000 0000 0000 0000 0000 0000
00000370: 0000 0000 0000 0000 0000 0000 0000 0000
00000380: 0000 0000 0000 0000 0000 0000 0000 0000
00000390: 0000 0000 0000 0000 0000 0000 0000 0000
000003a0: 0000 0000 0000 0000 0000 0000 0000 0000
000003b0: 0000 0000 0000 0000 0000 0000 0000 0000
000003c0: 0000 0000 0000 0000 0000 0000 0000 0000
000003d0: 0000 0000 0000 0000 0000 0000 0000 0000
000003e0: 0000 0000 0000 0000 0000 0000 0000 0000
000003f0: 0000 0000 0000 0000 0000 0000 0000 55aa
partition[0]: TYPE: 12, LBA = 0x1, #SECS = 0x20000
sd_init ok
init: starting sh
$ ls
.              1 1 1024
..             1 1 1024
README.md      2 2 493
cat            2 3 36440
echo           2 4 35040
forktest       2 5 16384
grep           2 6 40144
init           2 7 35544
kill           2 8 35064
ln             2 9 34944
ls             2 10 38544
mkdir          2 11 35088
rm             2 12 35080
sh             2 13 58752
stressfs       2 14 36120
usertests      2 15 166632
grind          2 16 49448
wc             2 17 37184
zombie         2 18 34560
blink          2 19 36040
pwm            2 20 35824
adc            2 21 35392
i2c            2 22 36256
spi            2 23 36272
console        3 24 0
$
```
