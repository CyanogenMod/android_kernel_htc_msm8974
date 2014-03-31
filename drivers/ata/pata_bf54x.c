/*
 * File:         drivers/ata/pata_bf54x.c
 * Author:       Sonic Zhang <sonic.zhang@analog.com>
 *
 * Created:
 * Description:  PATA Driver for blackfin 54x
 *
 * Modified:
 *               Copyright 2007 Analog Devices Inc.
 *
 * Bugs:         Enter bugs at http://blackfin.uclinux.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <linux/platform_device.h>
#include <asm/dma.h>
#include <asm/gpio.h>
#include <asm/portmux.h>

#define DRV_NAME		"pata-bf54x"
#define DRV_VERSION		"0.9"

#define ATA_REG_CTRL		0x0E
#define ATA_REG_ALTSTATUS	ATA_REG_CTRL

#define ATAPI_OFFSET_CONTROL		0x00
#define ATAPI_OFFSET_STATUS		0x04
#define ATAPI_OFFSET_DEV_ADDR		0x08
#define ATAPI_OFFSET_DEV_TXBUF		0x0c
#define ATAPI_OFFSET_DEV_RXBUF		0x10
#define ATAPI_OFFSET_INT_MASK		0x14
#define ATAPI_OFFSET_INT_STATUS		0x18
#define ATAPI_OFFSET_XFER_LEN		0x1c
#define ATAPI_OFFSET_LINE_STATUS	0x20
#define ATAPI_OFFSET_SM_STATE		0x24
#define ATAPI_OFFSET_TERMINATE		0x28
#define ATAPI_OFFSET_PIO_TFRCNT		0x2c
#define ATAPI_OFFSET_DMA_TFRCNT		0x30
#define ATAPI_OFFSET_UMAIN_TFRCNT	0x34
#define ATAPI_OFFSET_UDMAOUT_TFRCNT	0x38
#define ATAPI_OFFSET_REG_TIM_0		0x40
#define ATAPI_OFFSET_PIO_TIM_0		0x44
#define ATAPI_OFFSET_PIO_TIM_1		0x48
#define ATAPI_OFFSET_MULTI_TIM_0	0x50
#define ATAPI_OFFSET_MULTI_TIM_1	0x54
#define ATAPI_OFFSET_MULTI_TIM_2	0x58
#define ATAPI_OFFSET_ULTRA_TIM_0	0x60
#define ATAPI_OFFSET_ULTRA_TIM_1	0x64
#define ATAPI_OFFSET_ULTRA_TIM_2	0x68
#define ATAPI_OFFSET_ULTRA_TIM_3	0x6c


#define ATAPI_GET_CONTROL(base)\
	bfin_read16(base + ATAPI_OFFSET_CONTROL)
#define ATAPI_SET_CONTROL(base, val)\
	bfin_write16(base + ATAPI_OFFSET_CONTROL, val)
#define ATAPI_GET_STATUS(base)\
	bfin_read16(base + ATAPI_OFFSET_STATUS)
#define ATAPI_GET_DEV_ADDR(base)\
	bfin_read16(base + ATAPI_OFFSET_DEV_ADDR)
#define ATAPI_SET_DEV_ADDR(base, val)\
	bfin_write16(base + ATAPI_OFFSET_DEV_ADDR, val)
#define ATAPI_GET_DEV_TXBUF(base)\
	bfin_read16(base + ATAPI_OFFSET_DEV_TXBUF)
#define ATAPI_SET_DEV_TXBUF(base, val)\
	bfin_write16(base + ATAPI_OFFSET_DEV_TXBUF, val)
#define ATAPI_GET_DEV_RXBUF(base)\
	bfin_read16(base + ATAPI_OFFSET_DEV_RXBUF)
#define ATAPI_SET_DEV_RXBUF(base, val)\
	bfin_write16(base + ATAPI_OFFSET_DEV_RXBUF, val)
#define ATAPI_GET_INT_MASK(base)\
	bfin_read16(base + ATAPI_OFFSET_INT_MASK)
#define ATAPI_SET_INT_MASK(base, val)\
	bfin_write16(base + ATAPI_OFFSET_INT_MASK, val)
#define ATAPI_GET_INT_STATUS(base)\
	bfin_read16(base + ATAPI_OFFSET_INT_STATUS)
#define ATAPI_SET_INT_STATUS(base, val)\
	bfin_write16(base + ATAPI_OFFSET_INT_STATUS, val)
#define ATAPI_GET_XFER_LEN(base)\
	bfin_read16(base + ATAPI_OFFSET_XFER_LEN)
#define ATAPI_SET_XFER_LEN(base, val)\
	bfin_write16(base + ATAPI_OFFSET_XFER_LEN, val)
#define ATAPI_GET_LINE_STATUS(base)\
	bfin_read16(base + ATAPI_OFFSET_LINE_STATUS)
#define ATAPI_GET_SM_STATE(base)\
	bfin_read16(base + ATAPI_OFFSET_SM_STATE)
#define ATAPI_GET_TERMINATE(base)\
	bfin_read16(base + ATAPI_OFFSET_TERMINATE)
#define ATAPI_SET_TERMINATE(base, val)\
	bfin_write16(base + ATAPI_OFFSET_TERMINATE, val)
#define ATAPI_GET_PIO_TFRCNT(base)\
	bfin_read16(base + ATAPI_OFFSET_PIO_TFRCNT)
#define ATAPI_GET_DMA_TFRCNT(base)\
	bfin_read16(base + ATAPI_OFFSET_DMA_TFRCNT)
#define ATAPI_GET_UMAIN_TFRCNT(base)\
	bfin_read16(base + ATAPI_OFFSET_UMAIN_TFRCNT)
#define ATAPI_GET_UDMAOUT_TFRCNT(base)\
	bfin_read16(base + ATAPI_OFFSET_UDMAOUT_TFRCNT)
#define ATAPI_GET_REG_TIM_0(base)\
	bfin_read16(base + ATAPI_OFFSET_REG_TIM_0)
#define ATAPI_SET_REG_TIM_0(base, val)\
	bfin_write16(base + ATAPI_OFFSET_REG_TIM_0, val)
#define ATAPI_GET_PIO_TIM_0(base)\
	bfin_read16(base + ATAPI_OFFSET_PIO_TIM_0)
#define ATAPI_SET_PIO_TIM_0(base, val)\
	bfin_write16(base + ATAPI_OFFSET_PIO_TIM_0, val)
#define ATAPI_GET_PIO_TIM_1(base)\
	bfin_read16(base + ATAPI_OFFSET_PIO_TIM_1)
#define ATAPI_SET_PIO_TIM_1(base, val)\
	bfin_write16(base + ATAPI_OFFSET_PIO_TIM_1, val)
#define ATAPI_GET_MULTI_TIM_0(base)\
	bfin_read16(base + ATAPI_OFFSET_MULTI_TIM_0)
#define ATAPI_SET_MULTI_TIM_0(base, val)\
	bfin_write16(base + ATAPI_OFFSET_MULTI_TIM_0, val)
#define ATAPI_GET_MULTI_TIM_1(base)\
	bfin_read16(base + ATAPI_OFFSET_MULTI_TIM_1)
#define ATAPI_SET_MULTI_TIM_1(base, val)\
	bfin_write16(base + ATAPI_OFFSET_MULTI_TIM_1, val)
#define ATAPI_GET_MULTI_TIM_2(base)\
	bfin_read16(base + ATAPI_OFFSET_MULTI_TIM_2)
#define ATAPI_SET_MULTI_TIM_2(base, val)\
	bfin_write16(base + ATAPI_OFFSET_MULTI_TIM_2, val)
#define ATAPI_GET_ULTRA_TIM_0(base)\
	bfin_read16(base + ATAPI_OFFSET_ULTRA_TIM_0)
#define ATAPI_SET_ULTRA_TIM_0(base, val)\
	bfin_write16(base + ATAPI_OFFSET_ULTRA_TIM_0, val)
#define ATAPI_GET_ULTRA_TIM_1(base)\
	bfin_read16(base + ATAPI_OFFSET_ULTRA_TIM_1)
#define ATAPI_SET_ULTRA_TIM_1(base, val)\
	bfin_write16(base + ATAPI_OFFSET_ULTRA_TIM_1, val)
#define ATAPI_GET_ULTRA_TIM_2(base)\
	bfin_read16(base + ATAPI_OFFSET_ULTRA_TIM_2)
#define ATAPI_SET_ULTRA_TIM_2(base, val)\
	bfin_write16(base + ATAPI_OFFSET_ULTRA_TIM_2, val)
#define ATAPI_GET_ULTRA_TIM_3(base)\
	bfin_read16(base + ATAPI_OFFSET_ULTRA_TIM_3)
#define ATAPI_SET_ULTRA_TIM_3(base, val)\
	bfin_write16(base + ATAPI_OFFSET_ULTRA_TIM_3, val)

static const u32 pio_fsclk[] =
{ 33333333, 33333333, 33333333, 33333333, 33333333 };

static const u32 mdma_fsclk[] = { 33333333, 33333333, 33333333 };

static const u32 udma_fsclk[] =
{ 33333333, 33333333, 40000000, 50000000, 80000000, 133333333 };

static const u32 reg_t0min[]   = { 600, 383, 330, 180, 120 };
static const u32 reg_t2min[]   = { 290, 290, 290, 70,  25  };
static const u32 reg_teocmin[] = { 290, 290, 290, 80,  70  };

static const u32 pio_t0min[]   = { 600, 383, 240, 180, 120 };
static const u32 pio_t1min[]   = { 70,  50,  30,  30,  25  };
static const u32 pio_t2min[]   = { 165, 125, 100, 80,  70  };
static const u32 pio_teocmin[] = { 165, 125, 100, 70,  25  };
static const u32 pio_t4min[]   = { 30,  20,  15,  10,  10  };

static const u32 mdma_t0min[]  = { 480, 150, 120 };
static const u32 mdma_tdmin[]  = { 215, 80,  70  };
static const u32 mdma_thmin[]  = { 20,  15,  10  };
static const u32 mdma_tjmin[]  = { 20,  5,   5   };
static const u32 mdma_tkrmin[] = { 50,  50,  25  };
static const u32 mdma_tkwmin[] = { 215, 50,  25  };
static const u32 mdma_tmmin[]  = { 50,  30,  25  };
static const u32 mdma_tzmax[]  = { 20,  25,  25  };

static const u32 udma_tcycmin[]  = { 112, 73,  54,  39,  25,  17 };
static const u32 udma_tdvsmin[]  = { 70,  48,  31,  20,  7,   5  };
static const u32 udma_tenvmax[]  = { 70,  70,  70,  55,  55,  50 };
static const u32 udma_trpmin[]   = { 160, 125, 100, 100, 100, 85 };
static const u32 udma_tmin[]     = { 5,   5,   5,   5,   3,   3  };


static const u32 udma_tmlimin = 20;
static const u32 udma_tzahmin = 20;
static const u32 udma_tenvmin = 20;
static const u32 udma_tackmin = 20;
static const u32 udma_tssmin = 50;

#define BFIN_MAX_SG_SEGMENTS 4

static unsigned short num_clocks_min(unsigned long tmin,
				unsigned long fsclk)
{
	unsigned long tmp ;
	unsigned short result;

	tmp = tmin * (fsclk/1000/1000) / 1000;
	result = (unsigned short)tmp;
	if ((tmp*1000*1000) < (tmin*(fsclk/1000))) {
		result++;
	}

	return result;
}


static void bfin_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	int mode = adev->pio_mode - XFER_PIO_0;
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	unsigned int fsclk = get_sclk();
	unsigned short teoc_reg, t2_reg, teoc_pio;
	unsigned short t4_reg, t2_pio, t1_reg;
	unsigned short n0, n6, t6min = 5;

	n6 = num_clocks_min(t6min, fsclk);
	if (mode >= 0 && mode <= 4 && n6 >= 1) {
		dev_dbg(adev->link->ap->dev, "set piomode: mode=%d, fsclk=%ud\n", mode, fsclk);
		
		while (mode > 0 && pio_fsclk[mode] > fsclk)
			mode--;

		
		t2_reg = num_clocks_min(reg_t2min[mode], fsclk);
		
		teoc_reg = num_clocks_min(reg_teocmin[mode], fsclk);
		
		n0  = num_clocks_min(reg_t0min[mode], fsclk);

		
		if (t2_reg + teoc_reg < n0)
			t2_reg = n0 - teoc_reg;

		

		
		t2_pio = num_clocks_min(pio_t2min[mode], fsclk);
		
		teoc_pio = num_clocks_min(pio_teocmin[mode], fsclk);
		
		n0  = num_clocks_min(pio_t0min[mode], fsclk);

		
		if (t2_pio + teoc_pio < n0)
			t2_pio = n0 - teoc_pio;

		
		t1_reg = num_clocks_min(pio_t1min[mode], fsclk);

		
		t4_reg = num_clocks_min(pio_t4min[mode], fsclk);

		ATAPI_SET_REG_TIM_0(base, (teoc_reg<<8 | t2_reg));
		ATAPI_SET_PIO_TIM_0(base, (t4_reg<<12 | t2_pio<<4 | t1_reg));
		ATAPI_SET_PIO_TIM_1(base, teoc_pio);
		if (mode > 2) {
			ATAPI_SET_CONTROL(base,
				ATAPI_GET_CONTROL(base) | IORDY_EN);
		} else {
			ATAPI_SET_CONTROL(base,
				ATAPI_GET_CONTROL(base) & ~IORDY_EN);
		}

		
		ATAPI_SET_INT_MASK(base, ATAPI_GET_INT_MASK(base)
			& ~(PIO_DONE_MASK | HOST_TERM_XFER_MASK));
		SSYNC();
	}
}


static void bfin_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	int mode;
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	unsigned long fsclk = get_sclk();
	unsigned short tenv, tack, tcyc_tdvs, tdvs, tmli, tss, trp, tzah;
	unsigned short tm, td, tkr, tkw, teoc, th;
	unsigned short n0, nf, tfmin = 5;
	unsigned short nmin, tcyc;

	mode = adev->dma_mode - XFER_UDMA_0;
	if (mode >= 0 && mode <= 5) {
		dev_dbg(adev->link->ap->dev, "set udmamode: mode=%d\n", mode);
		while (mode > 0 && udma_fsclk[mode] > fsclk)
			mode--;

		nmin = num_clocks_min(udma_tmin[mode], fsclk);
		if (nmin >= 1) {
			
			tdvs = num_clocks_min(udma_tdvsmin[mode], fsclk);
			tcyc = num_clocks_min(udma_tcycmin[mode], fsclk);
			tcyc_tdvs = 2;

			if (tdvs + tcyc_tdvs < tcyc)
				tcyc_tdvs = tcyc - tdvs;

			if (tcyc_tdvs < 2)
				tcyc_tdvs = 2;

			if (tdvs < 2)
				tdvs = 2;

			tack = num_clocks_min(udma_tackmin, fsclk);
			tss = num_clocks_min(udma_tssmin, fsclk);
			tmli = num_clocks_min(udma_tmlimin, fsclk);
			tzah = num_clocks_min(udma_tzahmin, fsclk);
			trp = num_clocks_min(udma_trpmin[mode], fsclk);
			tenv = num_clocks_min(udma_tenvmin, fsclk);
			if (tenv <= udma_tenvmax[mode]) {
				ATAPI_SET_ULTRA_TIM_0(base, (tenv<<8 | tack));
				ATAPI_SET_ULTRA_TIM_1(base,
					(tcyc_tdvs<<8 | tdvs));
				ATAPI_SET_ULTRA_TIM_2(base, (tmli<<8 | tss));
				ATAPI_SET_ULTRA_TIM_3(base, (trp<<8 | tzah));
			}
		}
	}

	mode = adev->dma_mode - XFER_MW_DMA_0;
	if (mode >= 0 && mode <= 2) {
		dev_dbg(adev->link->ap->dev, "set mdmamode: mode=%d\n", mode);
		while (mode > 0 && mdma_fsclk[mode] > fsclk)
			mode--;

		nf = num_clocks_min(tfmin, fsclk);
		if (nf >= 1) {
			

			
			td = num_clocks_min(mdma_tdmin[mode], fsclk);

			
			tkw = num_clocks_min(mdma_tkwmin[mode], fsclk);

			
			n0  = num_clocks_min(mdma_t0min[mode], fsclk);

			
			if (tkw + td < n0)
				tkw = n0 - td;

			
			tkr = num_clocks_min(mdma_tkrmin[mode], fsclk);
			
			tm = num_clocks_min(mdma_tmmin[mode], fsclk);
			
			teoc = num_clocks_min(mdma_tjmin[mode], fsclk);
			
			th = num_clocks_min(mdma_thmin[mode], fsclk);

			ATAPI_SET_MULTI_TIM_0(base, (tm<<8 | td));
			ATAPI_SET_MULTI_TIM_1(base, (tkr<<8 | tkw));
			ATAPI_SET_MULTI_TIM_2(base, (teoc<<8 | th));
			SSYNC();
		}
	}
	return;
}

static inline void wait_complete(void __iomem *base, unsigned short mask)
{
	unsigned short status;
	unsigned int i = 0;

#define PATA_BF54X_WAIT_TIMEOUT		10000

	for (i = 0; i < PATA_BF54X_WAIT_TIMEOUT; i++) {
		status = ATAPI_GET_INT_STATUS(base) & mask;
		if (status)
			break;
	}

	ATAPI_SET_INT_STATUS(base, mask);
}


static void write_atapi_register(void __iomem *base,
		unsigned long ata_reg, unsigned short value)
{
	/* Program the ATA_DEV_TXBUF register with write data (to be
	 * written into the device).
	 */
	ATAPI_SET_DEV_TXBUF(base, value);

	ATAPI_SET_DEV_ADDR(base, ata_reg);

	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) | XFER_DIR));

	
	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) & ~PIO_USE_DMA));

	
	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) | PIO_START));

	wait_complete(base, PIO_DONE_INT);
}


static unsigned short read_atapi_register(void __iomem *base,
		unsigned long ata_reg)
{
	ATAPI_SET_DEV_ADDR(base, ata_reg);

	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) & ~XFER_DIR));

	
	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) & ~PIO_USE_DMA));

	
	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) | PIO_START));

	wait_complete(base, PIO_DONE_INT);

	/* Read the ATA_DEV_RXBUF register with write data (to be
	 * written into the device).
	 */
	return ATAPI_GET_DEV_RXBUF(base);
}


static void write_atapi_data(void __iomem *base,
		int len, unsigned short *buf)
{
	int i;

	
	ATAPI_SET_XFER_LEN(base, 1);

	ATAPI_SET_DEV_ADDR(base, ATA_REG_DATA);

	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) | XFER_DIR));

	
	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) & ~PIO_USE_DMA));

	for (i = 0; i < len; i++) {
		/* Program the ATA_DEV_TXBUF register with write data (to be
		 * written into the device).
		 */
		ATAPI_SET_DEV_TXBUF(base, buf[i]);

		
		ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) | PIO_START));

		wait_complete(base, PIO_DONE_INT);
	}
}


static void read_atapi_data(void __iomem *base,
		int len, unsigned short *buf)
{
	int i;

	
	ATAPI_SET_XFER_LEN(base, 1);

	ATAPI_SET_DEV_ADDR(base, ATA_REG_DATA);

	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) & ~XFER_DIR));

	
	ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) & ~PIO_USE_DMA));

	for (i = 0; i < len; i++) {
		
		ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base) | PIO_START));

		wait_complete(base, PIO_DONE_INT);

		/* Read the ATA_DEV_RXBUF register with write data (to be
		 * written into the device).
		 */
		buf[i] = ATAPI_GET_DEV_RXBUF(base);
	}
}


static void bfin_tf_load(struct ata_port *ap, const struct ata_taskfile *tf)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	unsigned int is_addr = tf->flags & ATA_TFLAG_ISADDR;

	if (tf->ctl != ap->last_ctl) {
		write_atapi_register(base, ATA_REG_CTRL, tf->ctl);
		ap->last_ctl = tf->ctl;
		ata_wait_idle(ap);
	}

	if (is_addr) {
		if (tf->flags & ATA_TFLAG_LBA48) {
			write_atapi_register(base, ATA_REG_FEATURE,
						tf->hob_feature);
			write_atapi_register(base, ATA_REG_NSECT,
						tf->hob_nsect);
			write_atapi_register(base, ATA_REG_LBAL, tf->hob_lbal);
			write_atapi_register(base, ATA_REG_LBAM, tf->hob_lbam);
			write_atapi_register(base, ATA_REG_LBAH, tf->hob_lbah);
			dev_dbg(ap->dev, "hob: feat 0x%X nsect 0x%X, lba 0x%X "
				 "0x%X 0x%X\n",
				tf->hob_feature,
				tf->hob_nsect,
				tf->hob_lbal,
				tf->hob_lbam,
				tf->hob_lbah);
		}

		write_atapi_register(base, ATA_REG_FEATURE, tf->feature);
		write_atapi_register(base, ATA_REG_NSECT, tf->nsect);
		write_atapi_register(base, ATA_REG_LBAL, tf->lbal);
		write_atapi_register(base, ATA_REG_LBAM, tf->lbam);
		write_atapi_register(base, ATA_REG_LBAH, tf->lbah);
		dev_dbg(ap->dev, "feat 0x%X nsect 0x%X lba 0x%X 0x%X 0x%X\n",
			tf->feature,
			tf->nsect,
			tf->lbal,
			tf->lbam,
			tf->lbah);
	}

	if (tf->flags & ATA_TFLAG_DEVICE) {
		write_atapi_register(base, ATA_REG_DEVICE, tf->device);
		dev_dbg(ap->dev, "device 0x%X\n", tf->device);
	}

	ata_wait_idle(ap);
}


static u8 bfin_check_status(struct ata_port *ap)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	return read_atapi_register(base, ATA_REG_STATUS);
}


static void bfin_tf_read(struct ata_port *ap, struct ata_taskfile *tf)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;

	tf->command = bfin_check_status(ap);
	tf->feature = read_atapi_register(base, ATA_REG_ERR);
	tf->nsect = read_atapi_register(base, ATA_REG_NSECT);
	tf->lbal = read_atapi_register(base, ATA_REG_LBAL);
	tf->lbam = read_atapi_register(base, ATA_REG_LBAM);
	tf->lbah = read_atapi_register(base, ATA_REG_LBAH);
	tf->device = read_atapi_register(base, ATA_REG_DEVICE);

	if (tf->flags & ATA_TFLAG_LBA48) {
		write_atapi_register(base, ATA_REG_CTRL, tf->ctl | ATA_HOB);
		tf->hob_feature = read_atapi_register(base, ATA_REG_ERR);
		tf->hob_nsect = read_atapi_register(base, ATA_REG_NSECT);
		tf->hob_lbal = read_atapi_register(base, ATA_REG_LBAL);
		tf->hob_lbam = read_atapi_register(base, ATA_REG_LBAM);
		tf->hob_lbah = read_atapi_register(base, ATA_REG_LBAH);
	}
}


static void bfin_exec_command(struct ata_port *ap,
			      const struct ata_taskfile *tf)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	dev_dbg(ap->dev, "ata%u: cmd 0x%X\n", ap->print_id, tf->command);

	write_atapi_register(base, ATA_REG_CMD, tf->command);
	ata_sff_pause(ap);
}


static u8 bfin_check_altstatus(struct ata_port *ap)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	return read_atapi_register(base, ATA_REG_ALTSTATUS);
}


static void bfin_dev_select(struct ata_port *ap, unsigned int device)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	u8 tmp;

	if (device == 0)
		tmp = ATA_DEVICE_OBS;
	else
		tmp = ATA_DEVICE_OBS | ATA_DEV1;

	write_atapi_register(base, ATA_REG_DEVICE, tmp);
	ata_sff_pause(ap);
}


static void bfin_set_devctl(struct ata_port *ap, u8 ctl)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	write_atapi_register(base, ATA_REG_CTRL, ctl);
}


static void bfin_bmdma_setup(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct dma_desc_array *dma_desc_cpu = (struct dma_desc_array *)ap->bmdma_prd;
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	unsigned short config = DMAFLOW_ARRAY | NDSIZE_5 | RESTART | WDSIZE_16 | DMAEN;
	struct scatterlist *sg;
	unsigned int si;
	unsigned int channel;
	unsigned int dir;
	unsigned int size = 0;

	dev_dbg(qc->ap->dev, "in atapi dma setup\n");
	
	if (qc->tf.flags & ATA_TFLAG_WRITE) {
		channel = CH_ATAPI_TX;
		dir = DMA_TO_DEVICE;
	} else {
		channel = CH_ATAPI_RX;
		dir = DMA_FROM_DEVICE;
		config |= WNR;
	}

	dma_map_sg(ap->dev, qc->sg, qc->n_elem, dir);

	
	for_each_sg(qc->sg, sg, qc->n_elem, si) {
		dma_desc_cpu[si].start_addr = sg_dma_address(sg);
		dma_desc_cpu[si].cfg = config;
		dma_desc_cpu[si].x_count = sg_dma_len(sg) >> 1;
		dma_desc_cpu[si].x_modify = 2;
		size += sg_dma_len(sg);
	}

	
	dma_desc_cpu[qc->n_elem - 1].cfg &= ~(DMAFLOW | NDSIZE);

	flush_dcache_range((unsigned int)dma_desc_cpu,
		(unsigned int)dma_desc_cpu +
			qc->n_elem * sizeof(struct dma_desc_array));

	
	set_dma_curr_desc_addr(channel, (unsigned long *)ap->bmdma_prd_dma);
	set_dma_x_count(channel, 0);
	set_dma_x_modify(channel, 0);
	set_dma_config(channel, config);

	SSYNC();

	
	bfin_exec_command(ap, &qc->tf);

	if (qc->tf.flags & ATA_TFLAG_WRITE) {
		
		ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base)
			| XFER_DIR));
	} else {
		
		ATAPI_SET_CONTROL(base, (ATAPI_GET_CONTROL(base)
			& ~XFER_DIR));
	}

	
	ATAPI_SET_CONTROL(base, ATAPI_GET_CONTROL(base) | TFRCNT_RST);

	
	ATAPI_SET_CONTROL(base, ATAPI_GET_CONTROL(base) | END_ON_TERM);

	
	ATAPI_SET_XFER_LEN(base, size >> 1);
}


static void bfin_bmdma_start(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;

	dev_dbg(qc->ap->dev, "in atapi dma start\n");

	if (!(ap->udma_mask || ap->mwdma_mask))
		return;

	
	if (ap->udma_mask)
		ATAPI_SET_CONTROL(base, ATAPI_GET_CONTROL(base)
			| ULTRA_START);
	else
		ATAPI_SET_CONTROL(base, ATAPI_GET_CONTROL(base)
			| MULTI_START);
}


static void bfin_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	unsigned int dir;

	dev_dbg(qc->ap->dev, "in atapi dma stop\n");

	if (!(ap->udma_mask || ap->mwdma_mask))
		return;

	
	if (qc->tf.flags & ATA_TFLAG_WRITE) {
		dir = DMA_TO_DEVICE;
		disable_dma(CH_ATAPI_TX);
	} else {
		dir = DMA_FROM_DEVICE;
		disable_dma(CH_ATAPI_RX);
	}

	dma_unmap_sg(ap->dev, qc->sg, qc->n_elem, dir);
}


static unsigned int bfin_devchk(struct ata_port *ap,
				unsigned int device)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	u8 nsect, lbal;

	bfin_dev_select(ap, device);

	write_atapi_register(base, ATA_REG_NSECT, 0x55);
	write_atapi_register(base, ATA_REG_LBAL, 0xaa);

	write_atapi_register(base, ATA_REG_NSECT, 0xaa);
	write_atapi_register(base, ATA_REG_LBAL, 0x55);

	write_atapi_register(base, ATA_REG_NSECT, 0x55);
	write_atapi_register(base, ATA_REG_LBAL, 0xaa);

	nsect = read_atapi_register(base, ATA_REG_NSECT);
	lbal = read_atapi_register(base, ATA_REG_LBAL);

	if ((nsect == 0x55) && (lbal == 0xaa))
		return 1;	

	return 0;		
}


static void bfin_bus_post_reset(struct ata_port *ap, unsigned int devmask)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	unsigned int dev0 = devmask & (1 << 0);
	unsigned int dev1 = devmask & (1 << 1);
	unsigned long deadline;

	if (dev0)
		ata_sff_busy_sleep(ap, ATA_TMOUT_BOOT_QUICK, ATA_TMOUT_BOOT);

	deadline = ata_deadline(jiffies, ATA_TMOUT_BOOT);
	while (dev1) {
		u8 nsect, lbal;

		bfin_dev_select(ap, 1);
		nsect = read_atapi_register(base, ATA_REG_NSECT);
		lbal = read_atapi_register(base, ATA_REG_LBAL);
		if ((nsect == 1) && (lbal == 1))
			break;
		if (time_after(jiffies, deadline)) {
			dev1 = 0;
			break;
		}
		ata_msleep(ap, 50);	
	}
	if (dev1)
		ata_sff_busy_sleep(ap, ATA_TMOUT_BOOT_QUICK, ATA_TMOUT_BOOT);

	
	bfin_dev_select(ap, 0);
	if (dev1)
		bfin_dev_select(ap, 1);
	if (dev0)
		bfin_dev_select(ap, 0);
}


static unsigned int bfin_bus_softreset(struct ata_port *ap,
				       unsigned int devmask)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;

	
	write_atapi_register(base, ATA_REG_CTRL, ap->ctl);
	udelay(20);
	write_atapi_register(base, ATA_REG_CTRL, ap->ctl | ATA_SRST);
	udelay(20);
	write_atapi_register(base, ATA_REG_CTRL, ap->ctl);

	/* spec mandates ">= 2ms" before checking status.
	 * We wait 150ms, because that was the magic delay used for
	 * ATAPI devices in Hale Landis's ATADRVR, for the period of time
	 * between when the ATA command register is written, and then
	 * status is checked.  Because waiting for "a while" before
	 * checking status is fine, post SRST, we perform this magic
	 * delay here as well.
	 *
	 * Old drivers/ide uses the 2mS rule and then waits for ready
	 */
	ata_msleep(ap, 150);

	if (bfin_check_status(ap) == 0xFF)
		return 0;

	bfin_bus_post_reset(ap, devmask);

	return 0;
}


static int bfin_softreset(struct ata_link *link, unsigned int *classes,
			  unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	unsigned int slave_possible = ap->flags & ATA_FLAG_SLAVE_POSS;
	unsigned int devmask = 0, err_mask;
	u8 err;

	
	if (bfin_devchk(ap, 0))
		devmask |= (1 << 0);
	if (slave_possible && bfin_devchk(ap, 1))
		devmask |= (1 << 1);

	
	bfin_dev_select(ap, 0);

	
	err_mask = bfin_bus_softreset(ap, devmask);
	if (err_mask) {
		ata_port_err(ap, "SRST failed (err_mask=0x%x)\n",
				err_mask);
		return -EIO;
	}

	
	classes[0] = ata_sff_dev_classify(&ap->link.device[0],
				devmask & (1 << 0), &err);
	if (slave_possible && err != 0x81)
		classes[1] = ata_sff_dev_classify(&ap->link.device[1],
					devmask & (1 << 1), &err);

	return 0;
}


static unsigned char bfin_bmdma_status(struct ata_port *ap)
{
	unsigned char host_stat = 0;
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;

	if (ATAPI_GET_STATUS(base) & (MULTI_XFER_ON | ULTRA_XFER_ON))
		host_stat |= ATA_DMA_ACTIVE;
	if (ATAPI_GET_INT_STATUS(base) & ATAPI_DEV_INT)
		host_stat |= ATA_DMA_INTR;

	dev_dbg(ap->dev, "ATAPI: host_stat=0x%x\n", host_stat);

	return host_stat;
}


static unsigned int bfin_data_xfer(struct ata_device *dev, unsigned char *buf,
				   unsigned int buflen, int rw)
{
	struct ata_port *ap = dev->link->ap;
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;
	unsigned int words = buflen >> 1;
	unsigned short *buf16 = (u16 *)buf;

	
	if (rw == READ)
		read_atapi_data(base, words, buf16);
	else
		write_atapi_data(base, words, buf16);

	
	if (unlikely(buflen & 0x01)) {
		unsigned short align_buf[1] = { 0 };
		unsigned char *trailing_buf = buf + buflen - 1;

		if (rw == READ) {
			read_atapi_data(base, 1, align_buf);
			memcpy(trailing_buf, align_buf, 1);
		} else {
			memcpy(align_buf, trailing_buf, 1);
			write_atapi_data(base, 1, align_buf);
		}
		words++;
	}

	return words << 1;
}


static void bfin_irq_clear(struct ata_port *ap)
{
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;

	dev_dbg(ap->dev, "in atapi irq clear\n");
	ATAPI_SET_INT_STATUS(base, ATAPI_GET_INT_STATUS(base)|ATAPI_DEV_INT
		| MULTI_DONE_INT | UDMAIN_DONE_INT | UDMAOUT_DONE_INT
		| MULTI_TERM_INT | UDMAIN_TERM_INT | UDMAOUT_TERM_INT);
}


void bfin_thaw(struct ata_port *ap)
{
	dev_dbg(ap->dev, "in atapi dma thaw\n");
	bfin_check_status(ap);
	ata_sff_irq_on(ap);
}


static void bfin_postreset(struct ata_link *link, unsigned int *classes)
{
	struct ata_port *ap = link->ap;
	void __iomem *base = (void __iomem *)ap->ioaddr.ctl_addr;

	
	ata_sff_irq_on(ap);

	
	if (classes[0] != ATA_DEV_NONE)
		bfin_dev_select(ap, 1);
	if (classes[1] != ATA_DEV_NONE)
		bfin_dev_select(ap, 0);

	
	if (classes[0] == ATA_DEV_NONE && classes[1] == ATA_DEV_NONE) {
		return;
	}

	
	write_atapi_register(base, ATA_REG_CTRL, ap->ctl);
}

static void bfin_port_stop(struct ata_port *ap)
{
	dev_dbg(ap->dev, "in atapi port stop\n");
	if (ap->udma_mask != 0 || ap->mwdma_mask != 0) {
		dma_free_coherent(ap->dev,
			BFIN_MAX_SG_SEGMENTS * sizeof(struct dma_desc_array),
			ap->bmdma_prd,
			ap->bmdma_prd_dma);

		free_dma(CH_ATAPI_RX);
		free_dma(CH_ATAPI_TX);
	}
}

static int bfin_port_start(struct ata_port *ap)
{
	dev_dbg(ap->dev, "in atapi port start\n");
	if (!(ap->udma_mask || ap->mwdma_mask))
		return 0;

	ap->bmdma_prd = dma_alloc_coherent(ap->dev,
				BFIN_MAX_SG_SEGMENTS * sizeof(struct dma_desc_array),
				&ap->bmdma_prd_dma,
				GFP_KERNEL);

	if (ap->bmdma_prd == NULL) {
		dev_info(ap->dev, "Unable to allocate DMA descriptor array.\n");
		goto out;
	}

	if (request_dma(CH_ATAPI_RX, "BFIN ATAPI RX DMA") >= 0) {
		if (request_dma(CH_ATAPI_TX,
			"BFIN ATAPI TX DMA") >= 0)
			return 0;

		free_dma(CH_ATAPI_RX);
		dma_free_coherent(ap->dev,
			BFIN_MAX_SG_SEGMENTS * sizeof(struct dma_desc_array),
			ap->bmdma_prd,
			ap->bmdma_prd_dma);
	}

out:
	ap->udma_mask = 0;
	ap->mwdma_mask = 0;
	dev_err(ap->dev, "Unable to request ATAPI DMA!"
		" Continue in PIO mode.\n");

	return 0;
}

static unsigned int bfin_ata_host_intr(struct ata_port *ap,
				   struct ata_queued_cmd *qc)
{
	struct ata_eh_info *ehi = &ap->link.eh_info;
	u8 status, host_stat = 0;

	VPRINTK("ata%u: protocol %d task_state %d\n",
		ap->print_id, qc->tf.protocol, ap->hsm_task_state);

	
	switch (ap->hsm_task_state) {
	case HSM_ST_FIRST:

		if (!(qc->dev->flags & ATA_DFLAG_CDB_INTR))
			goto idle_irq;
		break;
	case HSM_ST_LAST:
		if (qc->tf.protocol == ATA_PROT_DMA ||
		    qc->tf.protocol == ATAPI_PROT_DMA) {
			
			host_stat = ap->ops->bmdma_status(ap);
			VPRINTK("ata%u: host_stat 0x%X\n",
				ap->print_id, host_stat);

			
			if (!(host_stat & ATA_DMA_INTR))
				goto idle_irq;

			
			ap->ops->bmdma_stop(qc);

			if (unlikely(host_stat & ATA_DMA_ERR)) {
				
				qc->err_mask |= AC_ERR_HOST_BUS;
				ap->hsm_task_state = HSM_ST_ERR;
			}
		}
		break;
	case HSM_ST:
		break;
	default:
		goto idle_irq;
	}

	
	status = ap->ops->sff_check_altstatus(ap);
	if (status & ATA_BUSY)
		goto busy_ata;

	
	status = ap->ops->sff_check_status(ap);
	if (unlikely(status & ATA_BUSY))
		goto busy_ata;

	
	ap->ops->sff_irq_clear(ap);

	ata_sff_hsm_move(ap, qc, status, 0);

	if (unlikely(qc->err_mask) && (qc->tf.protocol == ATA_PROT_DMA ||
				       qc->tf.protocol == ATAPI_PROT_DMA))
		ata_ehi_push_desc(ehi, "BMDMA stat 0x%x", host_stat);

busy_ata:
	return 1;	

idle_irq:
	ap->stats.idle_irq++;

#ifdef ATA_IRQ_TRAP
	if ((ap->stats.idle_irq % 1000) == 0) {
		ap->ops->irq_ack(ap, 0); 
		ata_port_warn(ap, "irq trap\n");
		return 1;
	}
#endif
	return 0;	
}

static irqreturn_t bfin_ata_interrupt(int irq, void *dev_instance)
{
	struct ata_host *host = dev_instance;
	unsigned int i;
	unsigned int handled = 0;
	unsigned long flags;

	
	spin_lock_irqsave(&host->lock, flags);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		struct ata_queued_cmd *qc;

		qc = ata_qc_from_tag(ap, ap->link.active_tag);
		if (qc && (!(qc->tf.flags & ATA_TFLAG_POLLING)))
			handled |= bfin_ata_host_intr(ap, qc);
	}

	spin_unlock_irqrestore(&host->lock, flags);

	return IRQ_RETVAL(handled);
}


static struct scsi_host_template bfin_sht = {
	ATA_BASE_SHT(DRV_NAME),
	.sg_tablesize		= BFIN_MAX_SG_SEGMENTS,
	.dma_boundary		= ATA_DMA_BOUNDARY,
};

static struct ata_port_operations bfin_pata_ops = {
	.inherits		= &ata_bmdma_port_ops,

	.set_piomode		= bfin_set_piomode,
	.set_dmamode		= bfin_set_dmamode,

	.sff_tf_load		= bfin_tf_load,
	.sff_tf_read		= bfin_tf_read,
	.sff_exec_command	= bfin_exec_command,
	.sff_check_status	= bfin_check_status,
	.sff_check_altstatus	= bfin_check_altstatus,
	.sff_dev_select		= bfin_dev_select,
	.sff_set_devctl		= bfin_set_devctl,

	.bmdma_setup		= bfin_bmdma_setup,
	.bmdma_start		= bfin_bmdma_start,
	.bmdma_stop		= bfin_bmdma_stop,
	.bmdma_status		= bfin_bmdma_status,
	.sff_data_xfer		= bfin_data_xfer,

	.qc_prep		= ata_noop_qc_prep,

	.thaw			= bfin_thaw,
	.softreset		= bfin_softreset,
	.postreset		= bfin_postreset,

	.sff_irq_clear		= bfin_irq_clear,

	.port_start		= bfin_port_start,
	.port_stop		= bfin_port_stop,
};

static struct ata_port_info bfin_port_info[] = {
	{
		.flags		= ATA_FLAG_SLAVE_POSS,
		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= 0,
		.udma_mask	= 0,
		.port_ops	= &bfin_pata_ops,
	},
};


static int bfin_reset_controller(struct ata_host *host)
{
	void __iomem *base = (void __iomem *)host->ports[0]->ioaddr.ctl_addr;
	int count;
	unsigned short status;

	
	ATAPI_SET_INT_MASK(base, 0);
	SSYNC();

	
	ATAPI_SET_CONTROL(base, ATAPI_GET_CONTROL(base) | DEV_RST);
	udelay(30);

	
	ATAPI_SET_CONTROL(base, ATAPI_GET_CONTROL(base) & ~DEV_RST);
	msleep(2);

	
	count = 10000000;
	do {
		status = read_atapi_register(base, ATA_REG_STATUS);
	} while (--count && (status & ATA_BUSY));

	
	ATAPI_SET_INT_MASK(base, 1);
	SSYNC();

	return (!count);
}

static unsigned short atapi_io_port[] = {
	P_ATAPI_RESET,
	P_ATAPI_DIOR,
	P_ATAPI_DIOW,
	P_ATAPI_CS0,
	P_ATAPI_CS1,
	P_ATAPI_DMACK,
	P_ATAPI_DMARQ,
	P_ATAPI_INTRQ,
	P_ATAPI_IORDY,
	P_ATAPI_D0A,
	P_ATAPI_D1A,
	P_ATAPI_D2A,
	P_ATAPI_D3A,
	P_ATAPI_D4A,
	P_ATAPI_D5A,
	P_ATAPI_D6A,
	P_ATAPI_D7A,
	P_ATAPI_D8A,
	P_ATAPI_D9A,
	P_ATAPI_D10A,
	P_ATAPI_D11A,
	P_ATAPI_D12A,
	P_ATAPI_D13A,
	P_ATAPI_D14A,
	P_ATAPI_D15A,
	P_ATAPI_A0A,
	P_ATAPI_A1A,
	P_ATAPI_A2A,
	0
};

static int __devinit bfin_atapi_probe(struct platform_device *pdev)
{
	int board_idx = 0;
	struct resource *res;
	struct ata_host *host;
	unsigned int fsclk = get_sclk();
	int udma_mode = 5;
	const struct ata_port_info *ppi[] =
		{ &bfin_port_info[board_idx], NULL };

	if (unlikely(pdev->num_resources != 2)) {
		dev_err(&pdev->dev, "invalid number of resources\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -EINVAL;

	while (bfin_port_info[board_idx].udma_mask > 0 &&
			udma_fsclk[udma_mode] > fsclk) {
		udma_mode--;
		bfin_port_info[board_idx].udma_mask >>= 1;
	}

	host = ata_host_alloc_pinfo(&pdev->dev, ppi, 1);
	if (!host)
		return -ENOMEM;

	host->ports[0]->ioaddr.ctl_addr = (void *)res->start;

	if (peripheral_request_list(atapi_io_port, "atapi-io-port")) {
		dev_err(&pdev->dev, "Requesting Peripherals failed\n");
		return -EFAULT;
	}

	if (bfin_reset_controller(host)) {
		peripheral_free_list(atapi_io_port);
		dev_err(&pdev->dev, "Fail to reset ATAPI device\n");
		return -EFAULT;
	}

	if (ata_host_activate(host, platform_get_irq(pdev, 0),
		bfin_ata_interrupt, IRQF_SHARED, &bfin_sht) != 0) {
		peripheral_free_list(atapi_io_port);
		dev_err(&pdev->dev, "Fail to attach ATAPI device\n");
		return -ENODEV;
	}

	dev_set_drvdata(&pdev->dev, host);

	return 0;
}

static int __devexit bfin_atapi_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ata_host *host = dev_get_drvdata(dev);

	ata_host_detach(host);
	dev_set_drvdata(&pdev->dev, NULL);

	peripheral_free_list(atapi_io_port);

	return 0;
}

#ifdef CONFIG_PM
static int bfin_atapi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	if (host)
		return ata_host_suspend(host, state);
	else
		return 0;
}

static int bfin_atapi_resume(struct platform_device *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int ret;

	if (host) {
		ret = bfin_reset_controller(host);
		if (ret) {
			printk(KERN_ERR DRV_NAME ": Error during HW init\n");
			return ret;
		}
		ata_host_resume(host);
	}

	return 0;
}
#else
#define bfin_atapi_suspend NULL
#define bfin_atapi_resume NULL
#endif

static struct platform_driver bfin_atapi_driver = {
	.probe			= bfin_atapi_probe,
	.remove			= __devexit_p(bfin_atapi_remove),
	.suspend		= bfin_atapi_suspend,
	.resume			= bfin_atapi_resume,
	.driver = {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
	},
};

#define ATAPI_MODE_SIZE		10
static char bfin_atapi_mode[ATAPI_MODE_SIZE];

static int __init bfin_atapi_init(void)
{
	pr_info("register bfin atapi driver\n");

	switch(bfin_atapi_mode[0]) {
	case 'p':
	case 'P':
		break;
	case 'm':
	case 'M':
		bfin_port_info[0].mwdma_mask = ATA_MWDMA2;
		break;
	default:
		bfin_port_info[0].udma_mask = ATA_UDMA5;
	};

	return platform_driver_register(&bfin_atapi_driver);
}

static void __exit bfin_atapi_exit(void)
{
	platform_driver_unregister(&bfin_atapi_driver);
}

module_init(bfin_atapi_init);
module_exit(bfin_atapi_exit);
module_param_string(bfin_atapi_mode, bfin_atapi_mode, ATAPI_MODE_SIZE, 0);

MODULE_AUTHOR("Sonic Zhang <sonic.zhang@analog.com>");
MODULE_DESCRIPTION("PATA driver for blackfin 54x ATAPI controller");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:" DRV_NAME);
