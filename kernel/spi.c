// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org>
 * This driver reuses lots of code from u-boot/drivers/spi/designware_spi.c
 * Here is the copyright
 *
 * Designware master SPI core controller driver
 *
 * Copyright (C) 2014 Stefan Roese <sr@denx.de>
 * Copyright (C) 2020 Sean Anderson <seanga2@gmail.com>
 *
 * Very loosely based on the Linux driver:
 * drivers/spi/spi-dw.c, which is:
 * Copyright (c) 2009, Intel Corporation.
 */

#include "config.h"
#include "spi.h"
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


#define BIT(nr)		(1 << (nr))
typedef unsigned int	u32;
typedef unsigned short	u16;
typedef unsigned char	u8;

/*
 * min()/max()/clamp() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define min3(x, y, z) min((typeof(x))min(x, y), z)

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1: __min2; })

/* Register offsets */
#define DW_SPI_CTRLR0			0x00
#define DW_SPI_CTRLR1			0x04
#define DW_SPI_SSIENR			0x08
#define DW_SPI_MWCR			0x0c
#define DW_SPI_SER			0x10
#define DW_SPI_BAUDR			0x14
#define DW_SPI_TXFTLR			0x18
#define DW_SPI_RXFTLR			0x1c
#define DW_SPI_TXFLR			0x20
#define DW_SPI_RXFLR			0x24
#define DW_SPI_SR			0x28
#define DW_SPI_IMR			0x2c
#define DW_SPI_ISR			0x30
#define DW_SPI_RISR			0x34
#define DW_SPI_TXOICR			0x38
#define DW_SPI_RXOICR			0x3c
#define DW_SPI_RXUICR			0x40
#define DW_SPI_MSTICR			0x44
#define DW_SPI_ICR			0x48
#define DW_SPI_DMACR			0x4c
#define DW_SPI_DMATDLR			0x50
#define DW_SPI_DMARDLR			0x54
#define DW_SPI_IDR			0x58
#define DW_SPI_VERSION			0x5c
#define DW_SPI_DR			0x60

/* Bit fields in CTRLR0 */
/*
 * Only present when SSI_MAX_XFER_SIZE=16. This is the default, and the only
 * option before version 3.23a.
 */
#define CTRLR0_DFS_MASK			GENMASK(3, 0)

#define CTRLR0_FRF_MASK			GENMASK(5, 4)
#define CTRLR0_FRF_SPI			0x0
#define CTRLR0_FRF_SSP			0x1
#define CTRLR0_FRF_MICROWIRE		0x2
#define CTRLR0_FRF_RESV			0x3

#define CTRLR0_MODE_MASK		GENMASK(7, 6)
#define CTRLR0_MODE_SCPH		0x1
#define CTRLR0_MODE_SCPOL		0x2

#define CTRLR0_TMOD_MASK		GENMASK(9, 8)
#define	CTRLR0_TMOD_TR			0x0		/* xmit & recv */
#define CTRLR0_TMOD_TO			0x1		/* xmit only */
#define CTRLR0_TMOD_RO			0x2		/* recv only */
#define CTRLR0_TMOD_EPROMREAD		0x3		/* eeprom read mode */

#define CTRLR0_SLVOE_OFFSET		10
#define CTRLR0_SRL_OFFSET		11

/* The next field is only present on versions after 4.00a */
#define CTRLR0_SPI_FRF_BYTE		0x0
#define	CTRLR0_SPI_FRF_DUAL		0x1
#define	CTRLR0_SPI_FRF_QUAD		0x2

/* Bit fields in CTRLR0 based on DWC_ssi_databook.pdf v1.01a */
#define DWC_SSI_CTRLR0_SRL_OFFSET	13

/* Bit fields in SR, 7 bits */
#define SR_BUSY				BIT(0)
#define SR_TF_NOT_FULL			BIT(1)
#define SR_TF_EMPT			BIT(2)
#define SR_RF_NOT_EMPT			BIT(3)
#define SR_RF_FULL			BIT(4)
#define SR_TX_ERR			BIT(5)
#define SR_DCOL				BIT(6)

#define RX_TIMEOUT			1000		/* timeout in ms */

static struct dw_spi_priv {
	unsigned long regs;
	unsigned long bus_clk_rate;
	unsigned int freq;		/* Default frequency */
	unsigned int max_freq;
	unsigned int mode;

	const void *tx;
	const void *tx_end;
	void *rx;
	void *rx_end;
	u32 fifo_len;			/* depth of the FIFO buffer */
	u32 max_xfer;			/* Maximum transfer size (in bits) */

	int bits_per_word;
	int len;
	u8 cs;				/* chip select pin */
	u8 tmode;			/* TR/TO/RO/EEPROM */
	u8 type;			/* SPI/SSP/MicroWire */
} dw_spi_priv;

static inline u32 dw_read(struct dw_spi_priv *priv, u32 offset)
{
	return read32(priv->regs + offset);
}

static inline void dw_write(struct dw_spi_priv *priv, u32 offset, u32 val)
{
	write32(priv->regs + offset, val);
}

static u32 dw_spi_dw16_update_cr0(struct dw_spi_priv *priv)
{
	return ((priv->bits_per_word - 1) << 0) |
		(priv->type << 4) |
		(priv->mode << 6) |
		(priv->tmode << 8);
}

/* Restart the controller, disable all interrupts, clean rx fifo */
static void spi_hw_init(struct dw_spi_priv *priv)
{
	dw_write(priv, DW_SPI_SSIENR, 0);
	dw_write(priv, DW_SPI_IMR, 0);
	dw_write(priv, DW_SPI_SSIENR, 1);

	/*
	 * Try to detect the FIFO depth if not set by interface driver,
	 * the depth could be from 2 to 256 from HW spec
	 */
	if (!priv->fifo_len) {
		u32 fifo;

		for (fifo = 1; fifo < 256; fifo++) {
			dw_write(priv, DW_SPI_TXFTLR, fifo);
			if (fifo != dw_read(priv, DW_SPI_TXFTLR))
				break;
		}

		priv->fifo_len = (fifo == 1) ? 0 : fifo;
		dw_write(priv, DW_SPI_TXFTLR, 0);
	}
	//printf("fifo_len=%d\n", priv->fifo_len);
}

static int dw_spi_probe(struct dw_spi_priv *priv)
{
	priv->regs = SPI0;
	priv->max_freq = SPI0_MAX_FREQ;
	priv->freq = SPI0_MAX_FREQ;
	priv->bus_clk_rate = SPI0_CLK_FREQ;
	priv->max_xfer = 16;

	/* Currently only bits_per_word == 8 supported */
	priv->bits_per_word = 8;

	priv->tmode = 0; /* Tx & Rx */

	/* Basic HW init */
	spi_hw_init(priv);

	return 0;
}

/* Return the max entries we can fill into tx fifo */
static inline u32 tx_max(struct dw_spi_priv *priv)
{
	u32 tx_left, tx_room, rxtx_gap;

	tx_left = (priv->tx_end - priv->tx) / (priv->bits_per_word >> 3);
	tx_room = priv->fifo_len - dw_read(priv, DW_SPI_TXFLR);

	/*
	 * Another concern is about the tx/rx mismatch, we
	 * thought about using (priv->fifo_len - rxflr - txflr) as
	 * one maximum value for tx, but it doesn't cover the
	 * data which is out of tx/rx fifo and inside the
	 * shift registers. So a control from sw point of
	 * view is taken.
	 */
	rxtx_gap = ((priv->rx_end - priv->rx) - (priv->tx_end - priv->tx)) /
		(priv->bits_per_word >> 3);

	return min3(tx_left, tx_room, (u32)(priv->fifo_len - rxtx_gap));
}

/* Return the max entries we should read out of rx fifo */
static inline u32 rx_max(struct dw_spi_priv *priv)
{
	u32 rx_left = (priv->rx_end - priv->rx) / (priv->bits_per_word >> 3);

	return min_t(u32, rx_left, dw_read(priv, DW_SPI_RXFLR));
}

static void dw_writer(struct dw_spi_priv *priv)
{
	u32 max = tx_max(priv);
	u32 txw = 0xFFFFFFFF;

	while (max--) {
		/* Set the tx word if the transfer's original "tx" is not null */
		if (priv->tx_end - priv->len) {
			if (priv->bits_per_word == 8)
				txw = *(u8 *)(priv->tx);
			else
				txw = *(u16 *)(priv->tx);
		}
		dw_write(priv, DW_SPI_DR, txw);
		//printf("tx=0x%02x\n", txw);
		priv->tx += priv->bits_per_word >> 3;
	}
}

static void dw_reader(struct dw_spi_priv *priv)
{
	u32 max = rx_max(priv);
	u16 rxw;

	while (max--) {
		rxw = dw_read(priv, DW_SPI_DR);
		//printf("rx=0x%02x\n", rxw);

		/* Care about rx if the transfer's original "rx" is not null */
		if (priv->rx_end - priv->len) {
			if (priv->bits_per_word == 8)
				*(u8 *)(priv->rx) = rxw;
			else
				*(u16 *)(priv->rx) = rxw;
		}
		priv->rx += priv->bits_per_word >> 3;
	}
}

static int poll_transfer(struct dw_spi_priv *priv)
{
	do {
		dw_writer(priv);
		dw_reader(priv);
	} while (priv->rx_end > priv->rx);

	return 0;
}

static int dw_spi_xfer(struct dw_spi_priv *priv, unsigned int bitlen,
		       const void *dout, void *din)
{
	const u8 *tx = dout;
	u8 *rx = din;
	int ret = 0;
	u32 cr0 = 0;
	u32 val;
	u32 cs;
	int timeout = 1000000;

	/* spi core configured to do 8 bit transfers */
	if (bitlen % 8) {
		//dev_err(dev, "Non byte aligned SPI transfer.\n");
		return -1;
	}

	if (rx && tx)
		priv->tmode = CTRLR0_TMOD_TR;
	else if (rx)
		priv->tmode = CTRLR0_TMOD_RO;
	else
		/*
		 * In transmit only mode (CTRL0_TMOD_TO) input FIFO never gets
		 * any data which breaks our logic in poll_transfer() above.
		 */
		priv->tmode = CTRLR0_TMOD_TR;

	cr0 = dw_spi_dw16_update_cr0(priv);

	priv->len = bitlen >> 3;

	priv->tx = (void *)tx;
	priv->tx_end = priv->tx + priv->len;
	priv->rx = rx;
	priv->rx_end = priv->rx + priv->len;

	/* Disable controller before writing control registers */
	dw_write(priv, DW_SPI_SSIENR, 0);

	//printf("cr0=%08x rx=%p tx=%p len=%d [bytes]\n", cr0, rx, tx,
	//	priv->len);
	/* Reprogram cr0 only if changed */
	if (dw_read(priv, DW_SPI_CTRLR0) != cr0)
		dw_write(priv, DW_SPI_CTRLR0, cr0);

	/*
	 * Configure the desired SS (slave select 0...3) in the controller
	 * The DW SPI controller will activate and deactivate this CS
	 * automatically. So no cs_activate() etc is needed in this driver.
	 */
	//cs = spi_chip_select(dev);
	cs = 0;
	dw_write(priv, DW_SPI_SER, 1 << cs);

	/* Enable controller after writing control registers */
	dw_write(priv, DW_SPI_SSIENR, 1);

	/* Start transfer in a polling loop */
	ret = poll_transfer(priv);

	/*
	 * Wait for current transmit operation to complete.
	 * Otherwise if some data still exists in Tx FIFO it can be
	 * silently flushed, i.e. dropped on disabling of the controller,
	 * which happens when writing 0 to DW_SPI_SSIENR which happens
	 * in the beginning of new transfer.
	 */
	for (;;) {
		val = read32(priv->regs + DW_SPI_SR);
		if ((val & SR_TF_EMPT) && !(val & SR_BUSY))
			break;
		//kdelay(1);
		if (!timeout--)
			return -1;
	}

	return ret;
}

static int dw_spi_set_speed(struct dw_spi_priv *priv, unsigned int speed)
{
	u16 clk_div;

	if (speed > priv->max_freq)
		speed = priv->max_freq;

	/* Disable controller before writing control registers */
	dw_write(priv, DW_SPI_SSIENR, 0);

	/* clk_div doesn't support odd number */
	clk_div = priv->bus_clk_rate / speed;
	clk_div = (clk_div + 1) & 0xfffe;
	dw_write(priv, DW_SPI_BAUDR, clk_div);

	/* Enable controller after writing control registers */
	dw_write(priv, DW_SPI_SSIENR, 1);

	priv->freq = speed;
	//printf("speed=%d clk_div=%d\n", priv->freq, clk_div);

	return 0;
}

static int dw_spi_set_mode(struct dw_spi_priv *priv, unsigned int mode)
{
	/*
	 * Can't set mode yet. Since this depends on if rx, tx, or
	 * rx & tx is requested. So we have to defer this to the
	 * real transfer function.
	 */
	priv->mode = mode;

	return 0;
}

static int spi_xfer(struct dw_spi_priv *priv, struct spi_msg *argp)
{
	int ret;
	struct spi_msg msg;
	struct proc *p = myproc();
	u8 txbuf[32], rxbuf[32];

	ret = copyin(p->pagetable, (char *)&msg, (uint64)argp, sizeof(*argp));
	if (ret < 0)
		return ret;

	if (msg.len <= 0 || msg.len > 32)
		return -1;
	if (!msg.rx_buf && !msg.tx_buf)
		return -1;

	if (msg.tx_buf) {
		ret = copyin(p->pagetable, (char *)&txbuf, (uint64)msg.tx_buf, msg.len);
		if (ret < 0)
			return ret;
	}

	ret = dw_spi_xfer(priv, msg.len * 8, txbuf, rxbuf);
	if (ret < 0)
		return ret;

	if (msg.rx_buf) {
		ret = copyout(p->pagetable, (uint64)msg.rx_buf, (char *)&rxbuf, msg.len);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int spi_ioctl(int user_dst, unsigned long req, void *argp)
{
	switch(req) {
	case SPI_IOCTL_SET_SPEED:
		return dw_spi_set_speed(&dw_spi_priv, (unsigned long)argp);
	case SPI_IOCTL_SET_MODE:
		return dw_spi_set_mode(&dw_spi_priv, (unsigned long)argp);
	case SPI_IOCTL_TRANSFER:
		return spi_xfer(&dw_spi_priv, argp);
	default:
		return -1;
	}
}

void
spiinit(void)
{
	dw_spi_probe(&dw_spi_priv);
	devsw[SPI].ioctl = spi_ioctl;
}
#endif
