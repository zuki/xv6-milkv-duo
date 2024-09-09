// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org>
 *
 * This driver reuses lots of code from u-boot/drivers/i2c/designware_i2c.c
 * Here is the copyright
 * (C) Copyright 2009
 * Vipin Kumar, STMicroelectronics, vipin.kumar@st.com.
 */

#include "config.h"
#include "i2c.h"
#include "io.h"
#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "proc.h"

#ifdef I2C_DRIVER

typedef unsigned int		u32;
typedef unsigned char		u8;
typedef int			bool;
typedef unsigned int		uint32_t;
typedef unsigned short		uint16_t;
typedef unsigned char		uint8_t;
typedef int			int32_t;
typedef unsigned long		uintptr_t;

#define CONFIG_SYS_HZ		1000

#define BIT_I2C_CMD_DATA_READ_BIT           (0x01 << 8)
#define BIT_I2C_CMD_DATA_STOP_BIT           (0x01 << 9)

/* bit definition */
#define BIT_I2C_CON_MASTER_MODE              (0x01 << 0)
#define BIT_I2C_CON_STANDARD_SPEED           (0x01 << 1)
#define BIT_I2C_CON_FULL_SPEED               (0x02 << 1)
#define BIT_I2C_CON_HIGH_SPEED               (0x03 << 1)
#define BIT_I2C_CON_10B_ADDR_SLAVE           (0x01 << 3)
#define BIT_I2C_CON_10B_ADDR_MASTER          (0x01 << 4)
#define BIT_I2C_CON_RESTART_EN               (0x01 << 5)
#define BIT_I2C_CON_SLAVE_DIS                (0x01 << 6)

#define BIT_I2C_TAR_10B_ADDR_MASTER	 (0x01 << 12)

#define BIT_I2C_INT_RX_UNDER                 (0x01 << 0)
#define BIT_I2C_INT_RX_OVER                  (0x01 << 1)
#define BIT_I2C_INT_RX_FULL                  (0x01 << 2)
#define BIT_I2C_INT_TX_OVER                  (0x01 << 3)
#define BIT_I2C_INT_TX_EMPTY                 (0x01 << 4)
#define BIT_I2C_INT_RD_REQ                   (0x01 << 5)
#define BIT_I2C_INT_TX_ABRT                  (0x01 << 6)
#define BIT_I2C_INT_RX_DONE                  (0x01 << 7)
#define BIT_I2C_INT_ACTIVITY                 (0x01 << 8)
#define BIT_I2C_INT_STOP_DET                 (0x01 << 9)
#define BIT_I2C_INT_START_DET                (0x01 << 10)
#define BIT_I2C_INT_GEN_ALL                  (0x01 << 11)

struct i2c_regs {
	u32 ic_con;			/* 0x00 */
	u32 ic_tar;			/* 0x04 */
	u32 ic_sar;			/* 0x08 */
	u32 ic_hs_maddr;		/* 0x0c */
	u32 ic_cmd_data;		/* 0x10 */
	u32 ic_ss_scl_hcnt;		/* 0x14 */
	u32 ic_ss_scl_lcnt;		/* 0x18 */
	u32 ic_fs_scl_hcnt;		/* 0x1c */
	u32 ic_fs_scl_lcnt;		/* 0x20 */
	u32 ic_hs_scl_hcnt;		/* 0x24 */
	u32 ic_hs_scl_lcnt;		/* 0x28 */
	u32 ic_intr_stat;		/* 0x2c */
	u32 ic_intr_mask;		/* 0x30 */
	u32 ic_raw_intr_stat;		/* 0x34 */
	u32 ic_rx_tl;			/* 0x38 */
	u32 ic_tx_tl;			/* 0x3c */
	u32 ic_clr_intr;		/* 0x40 */
	u32 ic_clr_rx_under;		/* 0x44 */
	u32 ic_clr_rx_over;		/* 0x48 */
	u32 ic_clr_tx_over;		/* 0x4c */
	u32 ic_clr_rd_req;		/* 0x50 */
	u32 ic_clr_tx_abrt;		/* 0x54 */
	u32 ic_clr_rx_done;		/* 0x58 */
	u32 ic_clr_activity;		/* 0x5c */
	u32 ic_clr_stop_det;		/* 0x60 */
	u32 ic_clr_start_det;		/* 0x64 */
	u32 ic_clr_gen_call;		/* 0x68 */
	u32 ic_enable;			/* 0x6c */
	u32 ic_status;			/* 0x70 */
	u32 ic_txflr;			/* 0x74 */
	u32 ic_rxflr;			/* 0x78 */
	u32 ic_sda_hold;		/* 0x7c */
	u32 ic_tx_abrt_source;		/* 0x80 */
	u32 ic_slv_dat_nack_only;	/* 0x84 */
	u32 ic_dma_cr;			/* 0x88 */
	u32 ic_dma_tdlr;		/* 0x8c */
	u32 ic_dma_rdlr;		/* 0x90 */
	u32 ic_sda_setup;		/* 0x94 */
	u32 ic_ack_general_call;	/* 0x98 */
	u32 ic_enable_status;		/* 0x9c */
	u32 ic_fs_spklen;		/* 0xa0 */
	u32 ic_hs_spklen;		/* 0xa4 */
};

#define IC_CLK			100

/* High and low times in FS mode (in ns) */
#define MIN_FS_SCL_HIGHTIME	600
#define MIN_FS_SCL_LOWTIME	1300


/* Worst case timeout for 1 byte is kept as 2ms */
#define I2C_BYTE_TO		(CONFIG_SYS_HZ/500)
#define I2C_STOPDET_TO		(CONFIG_SYS_HZ/500)
#define I2C_BYTE_TO_BB		(I2C_BYTE_TO * 16)

/* i2c control register definitions */
#define IC_CON_SD		0x0040
#define IC_CON_RE		0x0020
#define IC_CON_10BITADDRMASTER	0x0010
#define IC_CON_10BITADDR_SLAVE	0x0008
#define IC_CON_SPD_MSK		0x0006
#define IC_CON_SPD_SS		0x0002
#define IC_CON_SPD_FS		0x0004
#define IC_CON_SPD_HS		0x0006
#define IC_CON_MM		0x0001

/* i2c data buffer and command register definitions */
#define IC_CMD			0x0100
#define IC_STOP			0x0200

/* i2c interrupt status register definitions */
#define IC_GEN_CALL		0x0800
#define IC_START_DET		0x0400
#define IC_STOP_DET		0x0200
#define IC_ACTIVITY		0x0100
#define IC_RX_DONE		0x0080
#define IC_TX_ABRT		0x0040
#define IC_RD_REQ		0x0020
#define IC_TX_EMPTY		0x0010
#define IC_TX_OVER		0x0008
#define IC_RX_FULL		0x0004
#define IC_RX_OVER 		0x0002
#define IC_RX_UNDER		0x0001

/* fifo threshold register definitions */
#define IC_TL0			0x00
#define IC_TL1			0x01
#define IC_TL2			0x02
#define IC_TL3			0x03
#define IC_TL4			0x04
#define IC_TL5			0x05
#define IC_TL6			0x06
#define IC_TL7			0x07
#define IC_RX_TL		IC_TL0
#define IC_TX_TL		IC_TL0

/* i2c enable register definitions */
#define IC_ENABLE		0x0001

/* i2c status register  definitions */
#define IC_STATUS_SA		0x0040
#define IC_STATUS_MA		0x0020
#define IC_STATUS_RFF		0x0010
#define IC_STATUS_RFNE		0x0008
#define IC_STATUS_TFE		0x0004
#define IC_STATUS_TFNF		0x0002
#define IC_STATUS_ACT		0x0001

#define I2C_MAX_SPEED		3400000
#define I2C_FAST_SPEED		400000
#define I2C_STANDARD_SPEED	100000

static struct i2c_regs *get_i2c_base(uint8_t i2c_id)
{
	return (struct i2c_regs *)I2C0;
}

static void dw_i2c_enable(struct i2c_regs *i2c, int enable)
{
	uint32_t ena = enable ? IC_ENABLE : 0;
	int timeout = 100;

	do {
		write32((uintptr_t)&i2c->ic_enable, ena);
		if ((read32((uintptr_t)&i2c->ic_enable_status) & IC_ENABLE) == ena)
			return;

		/*
		 * Wait 10 times the signaling period of the highest I2C
		 * transfer supported by the driver (for 400KHz this is
		 * 25us) as described in the DesignWare I2C databook.
		 */
		kdelay(1);
	} while (timeout--);

	printf("timeout in %sabling I2C adapter\n", enable ? "en" : "dis");
}

/*
 * i2c_setaddress - Sets the target slave address
 * @i2c_addr:	target i2c address
 *
 * Sets the target slave address.
 */
static void i2c_setaddress(struct i2c_regs *i2c, uint16_t i2c_addr)
{
	/* Disable i2c */
	dw_i2c_enable(i2c, 0);
	write32((uintptr_t)&i2c->ic_tar, i2c_addr);
	/* Enable i2c */
	dw_i2c_enable(i2c, 1);
}

/*
 * i2c_flush_rxfifo - Flushes the i2c RX FIFO
 *
 * Flushes the i2c RX FIFO
 */
static void i2c_flush_rxfifo(struct i2c_regs *i2c)
{
	while (read32((uintptr_t)&i2c->ic_status) & IC_STATUS_RFNE)
		read32((uintptr_t)&i2c->ic_cmd_data);
}

/*
 * i2c_wait_for_bb - Waits for bus busy
 *
 * Waits for bus busy
 */
static int i2c_wait_for_bb(struct i2c_regs *i2c)
{
	int timeout = 0;

	while ((read32((uintptr_t)&i2c->ic_status) & IC_STATUS_MA) ||
	       !(read32((uintptr_t)&i2c->ic_status) & IC_STATUS_TFE)) {

		/* Evaluate timeout */
		kdelay(1);
		timeout++;
		if (timeout > 200) /* exceed 1 ms */
			return 1;
	}

	return 0;
}

static int i2c_xfer_init(struct i2c_regs *i2c, uint8_t chip, uint32_t addr, int alen)
{
	if (i2c_wait_for_bb(i2c))
		return 1;

	i2c_setaddress(i2c, chip);
	while (alen) {
		alen--;
		/* high byte address going out first */
		write32((uintptr_t)&i2c->ic_cmd_data, (addr >> (alen * 8)) & 0xff);
	}
	return 0;
}

static int i2c_xfer_finish(struct i2c_regs *i2c)
{
	int timeout = 0;

	while (1) {
		if ((read32((uintptr_t)&i2c->ic_raw_intr_stat) & IC_STOP_DET)) {
			read32((uintptr_t)&i2c->ic_clr_stop_det);
			break;
		} else {
			timeout++;
			kdelay(1);
			if (timeout > I2C_STOPDET_TO) {
				printf("Timed out waiting for IC_STOP_DET\n");
				break;
			}
		}
	}

	if (i2c_wait_for_bb(i2c)) {
		printf("Timed out waiting for bus\n");
		return 1;
	}

	i2c_flush_rxfifo(i2c);

	return 0;
}

/*
 * i2c_read - Read from i2c memory
 * @chip:	target i2c address
 * @addr:	address to read from
 * @alen:
 * @buffer:	buffer for read data
 * @len:	no of bytes to be read
 *
 * Read from i2c memory.
 */
int dw_i2c_read(uint8_t i2c_id, uint8_t dev, uint16_t addr, uint16_t alen, uint8_t *buffer, uint16_t len)
{
	unsigned int active = 0;
	unsigned int time_count = 0;
	struct i2c_regs *i2c;
	int ret = 0;
	u32 val;

	i2c = get_i2c_base(i2c_id);

	dw_i2c_enable(i2c, 1);

	if (i2c_xfer_init(i2c, dev, addr, alen))
		return 1;

	while (len) {
		if (!active) {
			/*
			 * Avoid writing to ic_cmd_data multiple times
			 * in case this loop spins too quickly and the
			 * ic_status RFNE bit isn't set after the first
			 * write. Subsequent writes to ic_cmd_data can
			 * trigger spurious i2c transfer.
			 */
			write32((uintptr_t)&i2c->ic_cmd_data, (dev <<1) | BIT_I2C_CMD_DATA_READ_BIT | BIT_I2C_CMD_DATA_STOP_BIT);
			active = 1;
		}

		val = read32((uintptr_t)&i2c->ic_raw_intr_stat);
		if (val & BIT_I2C_INT_RX_FULL) {
			val = (uint8_t)read32((uintptr_t)&i2c->ic_cmd_data);
			*buffer++ = val;
			len--;
			time_count = 0;
			active = 0;
		} else {
			kdelay(1);
			time_count++;
			if (time_count  >= I2C_BYTE_TO )
				return 1;
		}
	}

	ret = i2c_xfer_finish(i2c);
	dw_i2c_enable(i2c, 0);

	return ret;
}

/*
 * i2c_write - Write to i2c memory
 * @chip:	target i2c address
 * @addr:	address to read from
 * @alen:
 * @buffer:	buffer for read data
 * @len:	no of bytes to be read
 *
 * Write to i2c memory.
 */

int dw_i2c_write(uint8_t i2c_id, uint8_t dev, uint16_t addr, uint16_t alen, uint8_t *buffer, uint16_t len)
{
	struct i2c_regs *i2c;
	int ret = 0;
	i2c = get_i2c_base(i2c_id);

	dw_i2c_enable(i2c, 1);

	if (i2c_xfer_init(i2c, dev, addr, alen))
		return 1;

	while (len) {
		if (i2c->ic_status & IC_STATUS_TFNF) {
			if (--len == 0) {
				write32((uintptr_t)&i2c->ic_cmd_data, *buffer | IC_STOP);
			} else {
				write32((uintptr_t)&i2c->ic_cmd_data, *buffer);
			}
			buffer++;
		} else
			printf("len=%d, ic status is not TFNF\n", len);
	}
	ret = i2c_xfer_finish(i2c);
	dw_i2c_enable(i2c, 0);
	return ret;
}

/*
 * i2c_set_bus_speed - Set the i2c speed
 * @speed:	required i2c speed
 *
 * Set the i2c speed.
 */
static void i2c_set_bus_speed(struct i2c_regs *i2c, unsigned int speed)
{
	unsigned int cntl;
	unsigned int hcnt, lcnt;

	/* to set speed cltr must be disabled */
	dw_i2c_enable(i2c, 0);

	cntl = (read32((uintptr_t)&i2c->ic_con) & (~IC_CON_SPD_MSK));

	cntl |= IC_CON_SPD_FS;
	hcnt = (uint16_t)(((IC_CLK * MIN_FS_SCL_HIGHTIME) / 1000) - 7);
	lcnt = (uint16_t)(((IC_CLK * MIN_FS_SCL_LOWTIME) / 1000) - 1);

	write32((uintptr_t)&i2c->ic_fs_scl_hcnt, hcnt);
	write32((uintptr_t)&i2c->ic_fs_scl_lcnt, lcnt);

	write32((uintptr_t)&i2c->ic_con, cntl);

	/* Enable back i2c now speed set */
	dw_i2c_enable(i2c, 1);
}

/*
 * dw_i2c_init - Init function
 * @speed:	required i2c speed
 * @slaveaddr:	slave address for the device
 *
 * Initialization function.
 */
void dw_i2c_init(uint8_t i2c_id)
{
	struct i2c_regs *i2c;

	/* Disable i2c */
	i2c = get_i2c_base(i2c_id);

	dw_i2c_enable(i2c, 0);
	write32((uintptr_t)&i2c->ic_con, (IC_CON_SD | IC_CON_SPD_FS | IC_CON_MM | IC_CON_RE));
	write32((uintptr_t)&i2c->ic_rx_tl, IC_RX_TL);
	write32((uintptr_t)&i2c->ic_tx_tl, IC_TX_TL);
	write32((uintptr_t)&i2c->ic_intr_mask, 0x0);
	i2c_set_bus_speed(i2c, I2C_FAST_SPEED);
}

static int i2c_read(struct i2c_msg *argp)
{
	int ret;
	struct i2c_msg msg;
	struct proc *p = myproc();
	u8 buf[64];

	ret = copyin(p->pagetable, (char *)&msg, (uint64)argp, sizeof(*argp));
	if (ret < 0)
		return ret;

	if (msg.len <= 0 || msg.len > 64)
		return -1;
	if (!msg.buf)
		return -1;

	ret = dw_i2c_read(0, msg.addr, msg.reg, 1, buf, msg.len);
	if (ret < 0)
		return ret;

	return copyout(p->pagetable, (uint64)msg.buf, (char *)&buf, msg.len);
}

static int i2c_write(struct i2c_msg *argp)
{
	int ret;
	struct i2c_msg msg;
	struct proc *p = myproc();
	u8 buf[64];

	ret = copyin(p->pagetable, (char *)&msg, (uint64)argp, sizeof(*argp));
	if (ret < 0)
		return ret;

	if (msg.len <= 0 || msg.len > 64)
		return -1;
	if (!msg.buf)
		return -1;

	ret = copyin(p->pagetable, (char *)&buf, (uint64)msg.buf, msg.len);
	if (ret < 0)
		return ret;

	return dw_i2c_write(0, msg.addr, msg.reg, 1, buf, msg.len);
}

static int i2c_ioctl(int user_dst, unsigned long req, void *argp)
{
	switch(req) {
	case I2C_IOCTL_RD:
		return i2c_read(argp);
	case I2C_IOCTL_WR:
		return i2c_write(argp);
	default:
		return -1;
	}
}

void
i2cinit(void)
{
	dw_i2c_init(0);
	devsw[I2C].ioctl = i2c_ioctl;
}

#endif
