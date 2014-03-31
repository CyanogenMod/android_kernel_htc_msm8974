/*
 * drivers/ata/sata_dwc_460ex.c
 *
 * Synopsys DesignWare Cores (DWC) SATA host driver
 *
 * Author: Mark Miesfeld <mmiesfeld@amcc.com>
 *
 * Ported from 2.6.19.2 to 2.6.25/26 by Stefan Roese <sr@denx.de>
 * Copyright 2008 DENX Software Engineering
 *
 * Based on versions provided by AMCC and Synopsys which are:
 *          Copyright 2006 Applied Micro Circuits Corporation
 *          COPYRIGHT (C) 2005  SYNOPSYS, INC.  ALL RIGHTS RESERVED
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifdef CONFIG_SATA_DWC_DEBUG
#define DEBUG
#endif

#ifdef CONFIG_SATA_DWC_VDEBUG
#define VERBOSE_DEBUG
#define DEBUG_NCQ
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/libata.h>
#include <linux/slab.h>
#include "libata.h"

#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>

#undef	DRV_NAME
#undef	DRV_VERSION
#define DRV_NAME        "sata-dwc"
#define DRV_VERSION     "1.3"

#define DMA_NUM_CHANS		1
#define DMA_NUM_CHAN_REGS	8

#define AHB_DMA_BRST_DFLT	64	

struct dmareg {
	u32 low;		
	u32 high;		
};

struct dma_chan_regs {
	struct dmareg sar;	
	struct dmareg dar;	
	struct dmareg llp;	
	struct dmareg ctl;	
	struct dmareg sstat;	
	struct dmareg dstat;	
	struct dmareg sstatar;	
	struct dmareg dstatar;	
	struct dmareg cfg;	
	struct dmareg sgr;	
	struct dmareg dsr;	
};

struct dma_interrupt_regs {
	struct dmareg tfr;	
	struct dmareg block;	
	struct dmareg srctran;	
	struct dmareg dsttran;	
	struct dmareg error;	
};

struct ahb_dma_regs {
	struct dma_chan_regs	chan_regs[DMA_NUM_CHAN_REGS];
	struct dma_interrupt_regs interrupt_raw;	
	struct dma_interrupt_regs interrupt_status;	
	struct dma_interrupt_regs interrupt_mask;	
	struct dma_interrupt_regs interrupt_clear;	
	struct dmareg		statusInt;	
	struct dmareg		rq_srcreg;	
	struct dmareg		rq_dstreg;	
	struct dmareg		rq_sgl_srcreg;	
	struct dmareg		rq_sgl_dstreg;	
	struct dmareg		rq_lst_srcreg;	
	struct dmareg		rq_lst_dstreg;	
	struct dmareg		dma_cfg;		
	struct dmareg		dma_chan_en;		
	struct dmareg		dma_id;			
	struct dmareg		dma_test;		
	struct dmareg		res1;			
	struct dmareg		res2;			
	struct dmareg		dma_params[6];
};

struct lli {
	u32		sar;		
	u32		dar;		
	u32		llp;		
	struct dmareg	ctl;		
	struct dmareg	dstat;		
};

enum {
	SATA_DWC_DMAC_LLI_SZ =	(sizeof(struct lli)),
	SATA_DWC_DMAC_LLI_NUM =	256,
	SATA_DWC_DMAC_LLI_TBL_SZ = (SATA_DWC_DMAC_LLI_SZ * \
					SATA_DWC_DMAC_LLI_NUM),
	SATA_DWC_DMAC_TWIDTH_BYTES = 4,
	SATA_DWC_DMAC_CTRL_TSIZE_MAX = (0x00000800 * \
						SATA_DWC_DMAC_TWIDTH_BYTES),
};

enum {
	DMA_EN	=		0x00000001, 
	DMA_CTL_LLP_SRCEN =	0x10000000, 
	DMA_CTL_LLP_DSTEN =	0x08000000, 
};

#define	DMA_CTL_BLK_TS(size)	((size) & 0x000000FFF)	
#define DMA_CHANNEL(ch)		(0x00000001 << (ch))	
	
#define	DMA_ENABLE_CHAN(ch)	((0x00000001 << (ch)) |			\
				 ((0x000000001 << (ch)) << 8))
	
#define	DMA_DISABLE_CHAN(ch)	(0x00000000 | ((0x000000001 << (ch)) << 8))
	
#define	DMA_CTL_TTFC(type)	(((type) & 0x7) << 20)
#define	DMA_CTL_SMS(num)	(((num) & 0x3) << 25) 
#define	DMA_CTL_DMS(num)	(((num) & 0x3) << 23)
	
#define DMA_CTL_SRC_MSIZE(size) (((size) & 0x7) << 14)
	
#define	DMA_CTL_DST_MSIZE(size) (((size) & 0x7) << 11)
	
#define	DMA_CTL_SRC_TRWID(size) (((size) & 0x7) << 4)
	
#define	DMA_CTL_DST_TRWID(size) (((size) & 0x7) << 1)

#define	DMA_CFG_HW_HS_DEST(int_num) (((int_num) & 0xF) << 11)
#define	DMA_CFG_HW_HS_SRC(int_num) (((int_num) & 0xF) << 7)
#define	DMA_LLP_LMS(addr, master) (((addr) & 0xfffffffc) | (master))

enum {
	DMA_CTL_LLP_DISABLE_LE32 = 0xffffffe7,
	DMA_CTL_TTFC_P2M_DMAC =	0x00000002, 
	DMA_CTL_TTFC_M2P_PER =	0x00000003, 
	DMA_CTL_SINC_INC =	0x00000000, 
	DMA_CTL_SINC_DEC =	0x00000200,
	DMA_CTL_SINC_NOCHANGE =	0x00000400,
	DMA_CTL_DINC_INC =	0x00000000, 
	DMA_CTL_DINC_DEC =	0x00000080,
	DMA_CTL_DINC_NOCHANGE =	0x00000100,
	DMA_CTL_INT_EN =	0x00000001, 

	DMA_CFG_FCMOD_REQ =	0x00000001, 
	DMA_CFG_PROTCTL	=	(0x00000003 << 2),

	DMA_CFG_RELD_DST =	0x80000000, 
	DMA_CFG_RELD_SRC =	0x40000000,
	DMA_CFG_HS_SELSRC =	0x00000800, 
	DMA_CFG_HS_SELDST =	0x00000400,
	DMA_CFG_FIFOEMPTY =     (0x00000001 << 9), 

	DMA_LLP_AHBMASTER1 =	0,	
	DMA_LLP_AHBMASTER2 =	1,

	SATA_DWC_MAX_PORTS = 1,

	SATA_DWC_SCR_OFFSET = 0x24,
	SATA_DWC_REG_OFFSET = 0x64,
};

struct sata_dwc_regs {
	u32 fptagr;		
	u32 fpbor;		
	u32 fptcr;		
	u32 dmacr;		
	u32 dbtsr;		
	u32 intpr;		
	u32 intmr;		
	u32 errmr;		
	u32 llcr;		
	u32 phycr;		
	u32 physr;		
	u32 rxbistpd;		
	u32 rxbistpd1;		
	u32 rxbistpd2;		
	u32 txbistpd;		
	u32 txbistpd1;		
	u32 txbistpd2;		
	u32 bistcr;		
	u32 bistfctr;		
	u32 bistsr;		
	u32 bistdecr;		
	u32 res[15];		
	u32 testr;		
	u32 versionr;		
	u32 idr;		
	u32 unimpl[192];	
	u32 dmadr[256];	
};

enum {
	SCR_SCONTROL_DET_ENABLE	=	0x00000001,
	SCR_SSTATUS_DET_PRESENT	=	0x00000001,
	SCR_SERROR_DIAG_X	=	0x04000000,
	SATA_DWC_TXFIFO_DEPTH	=	0x01FF,
	SATA_DWC_RXFIFO_DEPTH	=	0x01FF,
	SATA_DWC_DMACR_TMOD_TXCHEN =	0x00000004,
	SATA_DWC_DMACR_TXCHEN	= (0x00000001 | SATA_DWC_DMACR_TMOD_TXCHEN),
	SATA_DWC_DMACR_RXCHEN	= (0x00000002 | SATA_DWC_DMACR_TMOD_TXCHEN),
	SATA_DWC_DMACR_TXRXCH_CLEAR =	SATA_DWC_DMACR_TMOD_TXCHEN,
	SATA_DWC_INTPR_DMAT	=	0x00000001,
	SATA_DWC_INTPR_NEWFP	=	0x00000002,
	SATA_DWC_INTPR_PMABRT	=	0x00000004,
	SATA_DWC_INTPR_ERR	=	0x00000008,
	SATA_DWC_INTPR_NEWBIST	=	0x00000010,
	SATA_DWC_INTPR_IPF	=	0x10000000,
	SATA_DWC_INTMR_DMATM	=	0x00000001,
	SATA_DWC_INTMR_NEWFPM	=	0x00000002,
	SATA_DWC_INTMR_PMABRTM	=	0x00000004,
	SATA_DWC_INTMR_ERRM	=	0x00000008,
	SATA_DWC_INTMR_NEWBISTM	=	0x00000010,
	SATA_DWC_LLCR_SCRAMEN	=	0x00000001,
	SATA_DWC_LLCR_DESCRAMEN	=	0x00000002,
	SATA_DWC_LLCR_RPDEN	=	0x00000004,
	SATA_DWC_SERROR_ERR_BITS =	0x0FFF0F03
};

#define SATA_DWC_SCR0_SPD_GET(v)	(((v) >> 4) & 0x0000000F)
#define SATA_DWC_DMACR_TX_CLEAR(v)	(((v) & ~SATA_DWC_DMACR_TXCHEN) |\
						 SATA_DWC_DMACR_TMOD_TXCHEN)
#define SATA_DWC_DMACR_RX_CLEAR(v)	(((v) & ~SATA_DWC_DMACR_RXCHEN) |\
						 SATA_DWC_DMACR_TMOD_TXCHEN)
#define SATA_DWC_DBTSR_MWR(size)	(((size)/4) & SATA_DWC_TXFIFO_DEPTH)
#define SATA_DWC_DBTSR_MRD(size)	((((size)/4) & SATA_DWC_RXFIFO_DEPTH)\
						 << 16)
struct sata_dwc_device {
	struct device		*dev;		
	struct ata_probe_ent	*pe;		
	struct ata_host		*host;
	u8			*reg_base;
	struct sata_dwc_regs	*sata_dwc_regs;	
	int			irq_dma;
};

#define SATA_DWC_QCMD_MAX	32

struct sata_dwc_device_port {
	struct sata_dwc_device	*hsdev;
	int			cmd_issued[SATA_DWC_QCMD_MAX];
	struct lli		*llit[SATA_DWC_QCMD_MAX];  
	dma_addr_t		llit_dma[SATA_DWC_QCMD_MAX];
	u32			dma_chan[SATA_DWC_QCMD_MAX];
	int			dma_pending[SATA_DWC_QCMD_MAX];
};

#define HSDEV_FROM_HOST(host)  ((struct sata_dwc_device *)\
					(host)->private_data)
#define HSDEV_FROM_AP(ap)  ((struct sata_dwc_device *)\
					(ap)->host->private_data)
#define HSDEVP_FROM_AP(ap)   ((struct sata_dwc_device_port *)\
					(ap)->private_data)
#define HSDEV_FROM_QC(qc)	((struct sata_dwc_device *)\
					(qc)->ap->host->private_data)
#define HSDEV_FROM_HSDEVP(p)	((struct sata_dwc_device *)\
						(hsdevp)->hsdev)

enum {
	SATA_DWC_CMD_ISSUED_NOT		= 0,
	SATA_DWC_CMD_ISSUED_PEND	= 1,
	SATA_DWC_CMD_ISSUED_EXEC	= 2,
	SATA_DWC_CMD_ISSUED_NODATA	= 3,

	SATA_DWC_DMA_PENDING_NONE	= 0,
	SATA_DWC_DMA_PENDING_TX		= 1,
	SATA_DWC_DMA_PENDING_RX		= 2,
};

struct sata_dwc_host_priv {
	void	__iomem	 *scr_addr_sstatus;
	u32	sata_dwc_sactive_issued ;
	u32	sata_dwc_sactive_queued ;
	u32	dma_interrupt_count;
	struct	ahb_dma_regs	*sata_dma_regs;
	struct	device	*dwc_dev;
};
struct sata_dwc_host_priv host_pvt;
static void sata_dwc_bmdma_start_by_tag(struct ata_queued_cmd *qc, u8 tag);
static int sata_dwc_qc_complete(struct ata_port *ap, struct ata_queued_cmd *qc,
				u32 check_status);
static void sata_dwc_dma_xfer_complete(struct ata_port *ap, u32 check_status);
static void sata_dwc_port_stop(struct ata_port *ap);
static void sata_dwc_clear_dmacr(struct sata_dwc_device_port *hsdevp, u8 tag);
static int dma_dwc_init(struct sata_dwc_device *hsdev, int irq);
static void dma_dwc_exit(struct sata_dwc_device *hsdev);
static int dma_dwc_xfer_setup(struct scatterlist *sg, int num_elems,
			      struct lli *lli, dma_addr_t dma_lli,
			      void __iomem *addr, int dir);
static void dma_dwc_xfer_start(int dma_ch);

static const char *get_prot_descript(u8 protocol)
{
	switch ((enum ata_tf_protocols)protocol) {
	case ATA_PROT_NODATA:
		return "ATA no data";
	case ATA_PROT_PIO:
		return "ATA PIO";
	case ATA_PROT_DMA:
		return "ATA DMA";
	case ATA_PROT_NCQ:
		return "ATA NCQ";
	case ATAPI_PROT_NODATA:
		return "ATAPI no data";
	case ATAPI_PROT_PIO:
		return "ATAPI PIO";
	case ATAPI_PROT_DMA:
		return "ATAPI DMA";
	default:
		return "unknown";
	}
}

static const char *get_dma_dir_descript(int dma_dir)
{
	switch ((enum dma_data_direction)dma_dir) {
	case DMA_BIDIRECTIONAL:
		return "bidirectional";
	case DMA_TO_DEVICE:
		return "to device";
	case DMA_FROM_DEVICE:
		return "from device";
	default:
		return "none";
	}
}

static void sata_dwc_tf_dump(struct ata_taskfile *tf)
{
	dev_vdbg(host_pvt.dwc_dev, "taskfile cmd: 0x%02x protocol: %s flags:"
		"0x%lx device: %x\n", tf->command,
		get_prot_descript(tf->protocol), tf->flags, tf->device);
	dev_vdbg(host_pvt.dwc_dev, "feature: 0x%02x nsect: 0x%x lbal: 0x%x "
		"lbam: 0x%x lbah: 0x%x\n", tf->feature, tf->nsect, tf->lbal,
		 tf->lbam, tf->lbah);
	dev_vdbg(host_pvt.dwc_dev, "hob_feature: 0x%02x hob_nsect: 0x%x "
		"hob_lbal: 0x%x hob_lbam: 0x%x hob_lbah: 0x%x\n",
		tf->hob_feature, tf->hob_nsect, tf->hob_lbal, tf->hob_lbam,
		tf->hob_lbah);
}

static  int get_burst_length_encode(int datalength)
{
	int items = datalength >> 2;	

	if (items >= 64)
		return 5;

	if (items >= 32)
		return 4;

	if (items >= 16)
		return 3;

	if (items >= 8)
		return 2;

	if (items >= 4)
		return 1;

	return 0;
}

static  void clear_chan_interrupts(int c)
{
	out_le32(&(host_pvt.sata_dma_regs->interrupt_clear.tfr.low),
		 DMA_CHANNEL(c));
	out_le32(&(host_pvt.sata_dma_regs->interrupt_clear.block.low),
		 DMA_CHANNEL(c));
	out_le32(&(host_pvt.sata_dma_regs->interrupt_clear.srctran.low),
		 DMA_CHANNEL(c));
	out_le32(&(host_pvt.sata_dma_regs->interrupt_clear.dsttran.low),
		 DMA_CHANNEL(c));
	out_le32(&(host_pvt.sata_dma_regs->interrupt_clear.error.low),
		 DMA_CHANNEL(c));
}

static int dma_request_channel(void)
{
	int i;

	for (i = 0; i < DMA_NUM_CHANS; i++) {
		if (!(in_le32(&(host_pvt.sata_dma_regs->dma_chan_en.low)) &\
			DMA_CHANNEL(i)))
			return i;
	}
	dev_err(host_pvt.dwc_dev, "%s NO channel chan_en: 0x%08x\n", __func__,
		in_le32(&(host_pvt.sata_dma_regs->dma_chan_en.low)));
	return -1;
}

static irqreturn_t dma_dwc_interrupt(int irq, void *hsdev_instance)
{
	int chan;
	u32 tfr_reg, err_reg;
	unsigned long flags;
	struct sata_dwc_device *hsdev =
		(struct sata_dwc_device *)hsdev_instance;
	struct ata_host *host = (struct ata_host *)hsdev->host;
	struct ata_port *ap;
	struct sata_dwc_device_port *hsdevp;
	u8 tag = 0;
	unsigned int port = 0;

	spin_lock_irqsave(&host->lock, flags);
	ap = host->ports[port];
	hsdevp = HSDEVP_FROM_AP(ap);
	tag = ap->link.active_tag;

	tfr_reg = in_le32(&(host_pvt.sata_dma_regs->interrupt_status.tfr\
			.low));
	err_reg = in_le32(&(host_pvt.sata_dma_regs->interrupt_status.error\
			.low));

	dev_dbg(ap->dev, "eot=0x%08x err=0x%08x pending=%d active port=%d\n",
		tfr_reg, err_reg, hsdevp->dma_pending[tag], port);

	for (chan = 0; chan < DMA_NUM_CHANS; chan++) {
		
		if (tfr_reg & DMA_CHANNEL(chan)) {
			host_pvt.dma_interrupt_count++;
			sata_dwc_clear_dmacr(hsdevp, tag);

			if (hsdevp->dma_pending[tag] ==
			    SATA_DWC_DMA_PENDING_NONE) {
				dev_err(ap->dev, "DMA not pending eot=0x%08x "
					"err=0x%08x tag=0x%02x pending=%d\n",
					tfr_reg, err_reg, tag,
					hsdevp->dma_pending[tag]);
			}

			if ((host_pvt.dma_interrupt_count % 2) == 0)
				sata_dwc_dma_xfer_complete(ap, 1);

			
			out_le32(&(host_pvt.sata_dma_regs->interrupt_clear\
				.tfr.low),
				 DMA_CHANNEL(chan));
		}

		
		if (err_reg & DMA_CHANNEL(chan)) {
			
			dev_err(ap->dev, "error interrupt err_reg=0x%08x\n",
				err_reg);

			
			out_le32(&(host_pvt.sata_dma_regs->interrupt_clear\
				.error.low),
				 DMA_CHANNEL(chan));
		}
	}
	spin_unlock_irqrestore(&host->lock, flags);
	return IRQ_HANDLED;
}

static int dma_request_interrupts(struct sata_dwc_device *hsdev, int irq)
{
	int retval = 0;
	int chan;

	for (chan = 0; chan < DMA_NUM_CHANS; chan++) {
		
		out_le32(&(host_pvt.sata_dma_regs)->interrupt_mask.error.low,
			 DMA_ENABLE_CHAN(chan));

		
		out_le32(&(host_pvt.sata_dma_regs)->interrupt_mask.tfr.low,
			 DMA_ENABLE_CHAN(chan));
	}

	retval = request_irq(irq, dma_dwc_interrupt, 0, "SATA DMA", hsdev);
	if (retval) {
		dev_err(host_pvt.dwc_dev, "%s: could not get IRQ %d\n",
		__func__, irq);
		return -ENODEV;
	}

	
	hsdev->irq_dma = irq;
	return 0;
}

static int map_sg_to_lli(struct scatterlist *sg, int num_elems,
			struct lli *lli, dma_addr_t dma_lli,
			void __iomem *dmadr_addr, int dir)
{
	int i, idx = 0;
	int fis_len = 0;
	dma_addr_t next_llp;
	int bl;

	dev_dbg(host_pvt.dwc_dev, "%s: sg=%p nelem=%d lli=%p dma_lli=0x%08x"
		" dmadr=0x%08x\n", __func__, sg, num_elems, lli, (u32)dma_lli,
		(u32)dmadr_addr);

	bl = get_burst_length_encode(AHB_DMA_BRST_DFLT);

	for (i = 0; i < num_elems; i++, sg++) {
		u32 addr, offset;
		u32 sg_len, len;

		addr = (u32) sg_dma_address(sg);
		sg_len = sg_dma_len(sg);

		dev_dbg(host_pvt.dwc_dev, "%s: elem=%d sg_addr=0x%x sg_len"
			"=%d\n", __func__, i, addr, sg_len);

		while (sg_len) {
			if (idx >= SATA_DWC_DMAC_LLI_NUM) {
				
				dev_err(host_pvt.dwc_dev, "LLI table overrun "
				"(idx=%d)\n", idx);
				break;
			}
			len = (sg_len > SATA_DWC_DMAC_CTRL_TSIZE_MAX) ?
				SATA_DWC_DMAC_CTRL_TSIZE_MAX : sg_len;

			offset = addr & 0xffff;
			if ((offset + sg_len) > 0x10000)
				len = 0x10000 - offset;

			if (fis_len + len > 8192) {
				dev_dbg(host_pvt.dwc_dev, "SPLITTING: fis_len="
					"%d(0x%x) len=%d(0x%x)\n", fis_len,
					 fis_len, len, len);
				len = 8192 - fis_len;
				fis_len = 0;
			} else {
				fis_len += len;
			}
			if (fis_len == 8192)
				fis_len = 0;

			if (dir == DMA_FROM_DEVICE) {
				lli[idx].dar = cpu_to_le32(addr);
				lli[idx].sar = cpu_to_le32((u32)dmadr_addr);

				lli[idx].ctl.low = cpu_to_le32(
					DMA_CTL_TTFC(DMA_CTL_TTFC_P2M_DMAC) |
					DMA_CTL_SMS(0) |
					DMA_CTL_DMS(1) |
					DMA_CTL_SRC_MSIZE(bl) |
					DMA_CTL_DST_MSIZE(bl) |
					DMA_CTL_SINC_NOCHANGE |
					DMA_CTL_SRC_TRWID(2) |
					DMA_CTL_DST_TRWID(2) |
					DMA_CTL_INT_EN |
					DMA_CTL_LLP_SRCEN |
					DMA_CTL_LLP_DSTEN);
			} else {	
				lli[idx].sar = cpu_to_le32(addr);
				lli[idx].dar = cpu_to_le32((u32)dmadr_addr);

				lli[idx].ctl.low = cpu_to_le32(
					DMA_CTL_TTFC(DMA_CTL_TTFC_M2P_PER) |
					DMA_CTL_SMS(1) |
					DMA_CTL_DMS(0) |
					DMA_CTL_SRC_MSIZE(bl) |
					DMA_CTL_DST_MSIZE(bl) |
					DMA_CTL_DINC_NOCHANGE |
					DMA_CTL_SRC_TRWID(2) |
					DMA_CTL_DST_TRWID(2) |
					DMA_CTL_INT_EN |
					DMA_CTL_LLP_SRCEN |
					DMA_CTL_LLP_DSTEN);
			}

			dev_dbg(host_pvt.dwc_dev, "%s setting ctl.high len: "
				"0x%08x val: 0x%08x\n", __func__,
				len, DMA_CTL_BLK_TS(len / 4));

			
			lli[idx].ctl.high = cpu_to_le32(DMA_CTL_BLK_TS\
						(len / 4));

			next_llp = (dma_lli + ((idx + 1) * sizeof(struct \
							lli)));

			
			next_llp = DMA_LLP_LMS(next_llp, DMA_LLP_AHBMASTER2);

			lli[idx].llp = cpu_to_le32(next_llp);
			idx++;
			sg_len -= len;
			addr += len;
		}
	}

	if (idx) {
		lli[idx-1].llp = 0x00000000;
		lli[idx-1].ctl.low &= DMA_CTL_LLP_DISABLE_LE32;

		
		dma_cache_sync(NULL, lli, (sizeof(struct lli) * idx),
			       DMA_BIDIRECTIONAL);
	}

	return idx;
}

static void dma_dwc_xfer_start(int dma_ch)
{
	
	out_le32(&(host_pvt.sata_dma_regs->dma_chan_en.low),
		 in_le32(&(host_pvt.sata_dma_regs->dma_chan_en.low)) |
		 DMA_ENABLE_CHAN(dma_ch));
}

static int dma_dwc_xfer_setup(struct scatterlist *sg, int num_elems,
			      struct lli *lli, dma_addr_t dma_lli,
			      void __iomem *addr, int dir)
{
	int dma_ch;
	int num_lli;
	
	dma_ch = dma_request_channel();
	if (dma_ch == -1) {
		dev_err(host_pvt.dwc_dev, "%s: dma channel unavailable\n",
			 __func__);
		return -EAGAIN;
	}

	
	num_lli = map_sg_to_lli(sg, num_elems, lli, dma_lli, addr, dir);

	dev_dbg(host_pvt.dwc_dev, "%s sg: 0x%p, count: %d lli: %p dma_lli:"
		" 0x%0xlx addr: %p lli count: %d\n", __func__, sg, num_elems,
		 lli, (u32)dma_lli, addr, num_lli);

	clear_chan_interrupts(dma_ch);

	
	out_le32(&(host_pvt.sata_dma_regs->chan_regs[dma_ch].cfg.high),
		 DMA_CFG_PROTCTL | DMA_CFG_FCMOD_REQ);
	out_le32(&(host_pvt.sata_dma_regs->chan_regs[dma_ch].cfg.low), 0);

	
	out_le32(&(host_pvt.sata_dma_regs->chan_regs[dma_ch].llp.low),
		 DMA_LLP_LMS(dma_lli, DMA_LLP_AHBMASTER2));

	
	out_le32(&(host_pvt.sata_dma_regs->chan_regs[dma_ch].ctl.low),
		 DMA_CTL_LLP_SRCEN | DMA_CTL_LLP_DSTEN);
	return dma_ch;
}

static void dma_dwc_exit(struct sata_dwc_device *hsdev)
{
	dev_dbg(host_pvt.dwc_dev, "%s:\n", __func__);
	if (host_pvt.sata_dma_regs) {
		iounmap(host_pvt.sata_dma_regs);
		host_pvt.sata_dma_regs = NULL;
	}

	if (hsdev->irq_dma) {
		free_irq(hsdev->irq_dma, hsdev);
		hsdev->irq_dma = 0;
	}
}

static int dma_dwc_init(struct sata_dwc_device *hsdev, int irq)
{
	int err;

	err = dma_request_interrupts(hsdev, irq);
	if (err) {
		dev_err(host_pvt.dwc_dev, "%s: dma_request_interrupts returns"
			" %d\n", __func__, err);
		goto error_out;
	}

	
	out_le32(&(host_pvt.sata_dma_regs->dma_cfg.low), DMA_EN);

	dev_notice(host_pvt.dwc_dev, "DMA initialized\n");
	dev_dbg(host_pvt.dwc_dev, "SATA DMA registers=0x%p\n", host_pvt.\
		sata_dma_regs);

	return 0;

error_out:
	dma_dwc_exit(hsdev);

	return err;
}

static int sata_dwc_scr_read(struct ata_link *link, unsigned int scr, u32 *val)
{
	if (scr > SCR_NOTIFICATION) {
		dev_err(link->ap->dev, "%s: Incorrect SCR offset 0x%02x\n",
			__func__, scr);
		return -EINVAL;
	}

	*val = in_le32((void *)link->ap->ioaddr.scr_addr + (scr * 4));
	dev_dbg(link->ap->dev, "%s: id=%d reg=%d val=val=0x%08x\n",
		__func__, link->ap->print_id, scr, *val);

	return 0;
}

static int sata_dwc_scr_write(struct ata_link *link, unsigned int scr, u32 val)
{
	dev_dbg(link->ap->dev, "%s: id=%d reg=%d val=val=0x%08x\n",
		__func__, link->ap->print_id, scr, val);
	if (scr > SCR_NOTIFICATION) {
		dev_err(link->ap->dev, "%s: Incorrect SCR offset 0x%02x\n",
			 __func__, scr);
		return -EINVAL;
	}
	out_le32((void *)link->ap->ioaddr.scr_addr + (scr * 4), val);

	return 0;
}

static u32 core_scr_read(unsigned int scr)
{
	return in_le32((void __iomem *)(host_pvt.scr_addr_sstatus) +\
			(scr * 4));
}

static void core_scr_write(unsigned int scr, u32 val)
{
	out_le32((void __iomem *)(host_pvt.scr_addr_sstatus) + (scr * 4),
		val);
}

static void clear_serror(void)
{
	u32 val;
	val = core_scr_read(SCR_ERROR);
	core_scr_write(SCR_ERROR, val);

}

static void clear_interrupt_bit(struct sata_dwc_device *hsdev, u32 bit)
{
	out_le32(&hsdev->sata_dwc_regs->intpr,
		 in_le32(&hsdev->sata_dwc_regs->intpr));
}

static u32 qcmd_tag_to_mask(u8 tag)
{
	return 0x00000001 << (tag & 0x1f);
}

static void sata_dwc_error_intr(struct ata_port *ap,
				struct sata_dwc_device *hsdev, uint intpr)
{
	struct sata_dwc_device_port *hsdevp = HSDEVP_FROM_AP(ap);
	struct ata_eh_info *ehi = &ap->link.eh_info;
	unsigned int err_mask = 0, action = 0;
	struct ata_queued_cmd *qc;
	u32 serror;
	u8 status, tag;
	u32 err_reg;

	ata_ehi_clear_desc(ehi);

	serror = core_scr_read(SCR_ERROR);
	status = ap->ops->sff_check_status(ap);

	err_reg = in_le32(&(host_pvt.sata_dma_regs->interrupt_status.error.\
			low));
	tag = ap->link.active_tag;

	dev_err(ap->dev, "%s SCR_ERROR=0x%08x intpr=0x%08x status=0x%08x "
		"dma_intp=%d pending=%d issued=%d dma_err_status=0x%08x\n",
		__func__, serror, intpr, status, host_pvt.dma_interrupt_count,
		hsdevp->dma_pending[tag], hsdevp->cmd_issued[tag], err_reg);

	
	clear_serror();
	clear_interrupt_bit(hsdev, SATA_DWC_INTPR_ERR);

	

	err_mask |= AC_ERR_HOST_BUS;
	action |= ATA_EH_RESET;

	
	ehi->serror |= serror;
	ehi->action |= action;

	qc = ata_qc_from_tag(ap, tag);
	if (qc)
		qc->err_mask |= err_mask;
	else
		ehi->err_mask |= err_mask;

	ata_port_abort(ap);
}

static irqreturn_t sata_dwc_isr(int irq, void *dev_instance)
{
	struct ata_host *host = (struct ata_host *)dev_instance;
	struct sata_dwc_device *hsdev = HSDEV_FROM_HOST(host);
	struct ata_port *ap;
	struct ata_queued_cmd *qc;
	unsigned long flags;
	u8 status, tag;
	int handled, num_processed, port = 0;
	uint intpr, sactive, sactive2, tag_mask;
	struct sata_dwc_device_port *hsdevp;
	host_pvt.sata_dwc_sactive_issued = 0;

	spin_lock_irqsave(&host->lock, flags);

	
	intpr = in_le32(&hsdev->sata_dwc_regs->intpr);

	ap = host->ports[port];
	hsdevp = HSDEVP_FROM_AP(ap);

	dev_dbg(ap->dev, "%s intpr=0x%08x active_tag=%d\n", __func__, intpr,
		ap->link.active_tag);

	
	if (intpr & SATA_DWC_INTPR_ERR) {
		sata_dwc_error_intr(ap, hsdev, intpr);
		handled = 1;
		goto DONE;
	}

	
	if (intpr & SATA_DWC_INTPR_NEWFP) {
		clear_interrupt_bit(hsdev, SATA_DWC_INTPR_NEWFP);

		tag = (u8)(in_le32(&hsdev->sata_dwc_regs->fptagr));
		dev_dbg(ap->dev, "%s: NEWFP tag=%d\n", __func__, tag);
		if (hsdevp->cmd_issued[tag] != SATA_DWC_CMD_ISSUED_PEND)
			dev_warn(ap->dev, "CMD tag=%d not pending?\n", tag);

		host_pvt.sata_dwc_sactive_issued |= qcmd_tag_to_mask(tag);

		qc = ata_qc_from_tag(ap, tag);
		qc->ap->link.active_tag = tag;
		sata_dwc_bmdma_start_by_tag(qc, tag);

		handled = 1;
		goto DONE;
	}
	sactive = core_scr_read(SCR_ACTIVE);
	tag_mask = (host_pvt.sata_dwc_sactive_issued | sactive) ^ sactive;

	
	if (host_pvt.sata_dwc_sactive_issued == 0 && tag_mask == 0) {
		if (ap->link.active_tag == ATA_TAG_POISON)
			tag = 0;
		else
			tag = ap->link.active_tag;
		qc = ata_qc_from_tag(ap, tag);

		
		if (unlikely(!qc || (qc->tf.flags & ATA_TFLAG_POLLING))) {
			dev_err(ap->dev, "%s interrupt with no active qc "
				"qc=%p\n", __func__, qc);
			ap->ops->sff_check_status(ap);
			handled = 1;
			goto DONE;
		}
		status = ap->ops->sff_check_status(ap);

		qc->ap->link.active_tag = tag;
		hsdevp->cmd_issued[tag] = SATA_DWC_CMD_ISSUED_NOT;

		if (status & ATA_ERR) {
			dev_dbg(ap->dev, "interrupt ATA_ERR (0x%x)\n", status);
			sata_dwc_qc_complete(ap, qc, 1);
			handled = 1;
			goto DONE;
		}

		dev_dbg(ap->dev, "%s non-NCQ cmd interrupt, protocol: %s\n",
			__func__, get_prot_descript(qc->tf.protocol));
DRVSTILLBUSY:
		if (ata_is_dma(qc->tf.protocol)) {
			host_pvt.dma_interrupt_count++;
			if (hsdevp->dma_pending[tag] == \
					SATA_DWC_DMA_PENDING_NONE) {
				dev_err(ap->dev, "%s: DMA not pending "
					"intpr=0x%08x status=0x%08x pending"
					"=%d\n", __func__, intpr, status,
					hsdevp->dma_pending[tag]);
			}

			if ((host_pvt.dma_interrupt_count % 2) == 0)
				sata_dwc_dma_xfer_complete(ap, 1);
		} else if (ata_is_pio(qc->tf.protocol)) {
			ata_sff_hsm_move(ap, qc, status, 0);
			handled = 1;
			goto DONE;
		} else {
			if (unlikely(sata_dwc_qc_complete(ap, qc, 1)))
				goto DRVSTILLBUSY;
		}

		handled = 1;
		goto DONE;
	}


	 
	sactive = core_scr_read(SCR_ACTIVE);
	tag_mask = (host_pvt.sata_dwc_sactive_issued | sactive) ^ sactive;

	if (sactive != 0 || (host_pvt.sata_dwc_sactive_issued) > 1 || \
							tag_mask > 1) {
		dev_dbg(ap->dev, "%s NCQ:sactive=0x%08x  sactive_issued=0x%08x"
			"tag_mask=0x%08x\n", __func__, sactive,
			host_pvt.sata_dwc_sactive_issued, tag_mask);
	}

	if ((tag_mask | (host_pvt.sata_dwc_sactive_issued)) != \
					(host_pvt.sata_dwc_sactive_issued)) {
		dev_warn(ap->dev, "Bad tag mask?  sactive=0x%08x "
			 "(host_pvt.sata_dwc_sactive_issued)=0x%08x  tag_mask"
			 "=0x%08x\n", sactive, host_pvt.sata_dwc_sactive_issued,
			  tag_mask);
	}

	
	status = ap->ops->sff_check_status(ap);
	dev_dbg(ap->dev, "%s ATA status register=0x%x\n", __func__, status);

	tag = 0;
	num_processed = 0;
	while (tag_mask) {
		num_processed++;
		while (!(tag_mask & 0x00000001)) {
			tag++;
			tag_mask <<= 1;
		}

		tag_mask &= (~0x00000001);
		qc = ata_qc_from_tag(ap, tag);

		
		qc->ap->link.active_tag = tag;
		hsdevp->cmd_issued[tag] = SATA_DWC_CMD_ISSUED_NOT;

		
		if (status & ATA_ERR) {
			dev_dbg(ap->dev, "%s ATA_ERR (0x%x)\n", __func__,
				status);
			sata_dwc_qc_complete(ap, qc, 1);
			handled = 1;
			goto DONE;
		}

		
		dev_dbg(ap->dev, "%s NCQ command, protocol: %s\n", __func__,
			get_prot_descript(qc->tf.protocol));
		if (ata_is_dma(qc->tf.protocol)) {
			host_pvt.dma_interrupt_count++;
			if (hsdevp->dma_pending[tag] == \
					SATA_DWC_DMA_PENDING_NONE)
				dev_warn(ap->dev, "%s: DMA not pending?\n",
					__func__);
			if ((host_pvt.dma_interrupt_count % 2) == 0)
				sata_dwc_dma_xfer_complete(ap, 1);
		} else {
			if (unlikely(sata_dwc_qc_complete(ap, qc, 1)))
				goto STILLBUSY;
		}
		continue;

STILLBUSY:
		ap->stats.idle_irq++;
		dev_warn(ap->dev, "STILL BUSY IRQ ata%d: irq trap\n",
			ap->print_id);
	} 

	sactive2 = core_scr_read(SCR_ACTIVE);
	if (sactive2 != sactive) {
		dev_dbg(ap->dev, "More completed - sactive=0x%x sactive2"
			"=0x%x\n", sactive, sactive2);
	}
	handled = 1;

DONE:
	spin_unlock_irqrestore(&host->lock, flags);
	return IRQ_RETVAL(handled);
}

static void sata_dwc_clear_dmacr(struct sata_dwc_device_port *hsdevp, u8 tag)
{
	struct sata_dwc_device *hsdev = HSDEV_FROM_HSDEVP(hsdevp);

	if (hsdevp->dma_pending[tag] == SATA_DWC_DMA_PENDING_RX) {
		out_le32(&(hsdev->sata_dwc_regs->dmacr),
			 SATA_DWC_DMACR_RX_CLEAR(
				 in_le32(&(hsdev->sata_dwc_regs->dmacr))));
	} else if (hsdevp->dma_pending[tag] == SATA_DWC_DMA_PENDING_TX) {
		out_le32(&(hsdev->sata_dwc_regs->dmacr),
			 SATA_DWC_DMACR_TX_CLEAR(
				 in_le32(&(hsdev->sata_dwc_regs->dmacr))));
	} else {
		dev_err(host_pvt.dwc_dev, "%s DMA protocol RX and"
			"TX DMA not pending tag=0x%02x pending=%d"
			" dmacr: 0x%08x\n", __func__, tag,
			hsdevp->dma_pending[tag],
			in_le32(&(hsdev->sata_dwc_regs->dmacr)));
		out_le32(&(hsdev->sata_dwc_regs->dmacr),
			SATA_DWC_DMACR_TXRXCH_CLEAR);
	}
}

static void sata_dwc_dma_xfer_complete(struct ata_port *ap, u32 check_status)
{
	struct ata_queued_cmd *qc;
	struct sata_dwc_device_port *hsdevp = HSDEVP_FROM_AP(ap);
	struct sata_dwc_device *hsdev = HSDEV_FROM_AP(ap);
	u8 tag = 0;

	tag = ap->link.active_tag;
	qc = ata_qc_from_tag(ap, tag);
	if (!qc) {
		dev_err(ap->dev, "failed to get qc");
		return;
	}

#ifdef DEBUG_NCQ
	if (tag > 0) {
		dev_info(ap->dev, "%s tag=%u cmd=0x%02x dma dir=%s proto=%s "
			 "dmacr=0x%08x\n", __func__, qc->tag, qc->tf.command,
			 get_dma_dir_descript(qc->dma_dir),
			 get_prot_descript(qc->tf.protocol),
			 in_le32(&(hsdev->sata_dwc_regs->dmacr)));
	}
#endif

	if (ata_is_dma(qc->tf.protocol)) {
		if (hsdevp->dma_pending[tag] == SATA_DWC_DMA_PENDING_NONE) {
			dev_err(ap->dev, "%s DMA protocol RX and TX DMA not "
				"pending dmacr: 0x%08x\n", __func__,
				in_le32(&(hsdev->sata_dwc_regs->dmacr)));
		}

		hsdevp->dma_pending[tag] = SATA_DWC_DMA_PENDING_NONE;
		sata_dwc_qc_complete(ap, qc, check_status);
		ap->link.active_tag = ATA_TAG_POISON;
	} else {
		sata_dwc_qc_complete(ap, qc, check_status);
	}
}

static int sata_dwc_qc_complete(struct ata_port *ap, struct ata_queued_cmd *qc,
				u32 check_status)
{
	u8 status = 0;
	u32 mask = 0x0;
	u8 tag = qc->tag;
	struct sata_dwc_device_port *hsdevp = HSDEVP_FROM_AP(ap);
	host_pvt.sata_dwc_sactive_queued = 0;
	dev_dbg(ap->dev, "%s checkstatus? %x\n", __func__, check_status);

	if (hsdevp->dma_pending[tag] == SATA_DWC_DMA_PENDING_TX)
		dev_err(ap->dev, "TX DMA PENDING\n");
	else if (hsdevp->dma_pending[tag] == SATA_DWC_DMA_PENDING_RX)
		dev_err(ap->dev, "RX DMA PENDING\n");
	dev_dbg(ap->dev, "QC complete cmd=0x%02x status=0x%02x ata%u:"
		" protocol=%d\n", qc->tf.command, status, ap->print_id,
		 qc->tf.protocol);

	
	mask = (~(qcmd_tag_to_mask(tag)));
	host_pvt.sata_dwc_sactive_queued = (host_pvt.sata_dwc_sactive_queued) \
						& mask;
	host_pvt.sata_dwc_sactive_issued = (host_pvt.sata_dwc_sactive_issued) \
						& mask;
	ata_qc_complete(qc);
	return 0;
}

static void sata_dwc_enable_interrupts(struct sata_dwc_device *hsdev)
{
	
	out_le32(&hsdev->sata_dwc_regs->intmr,
		 SATA_DWC_INTMR_ERRM |
		 SATA_DWC_INTMR_NEWFPM |
		 SATA_DWC_INTMR_PMABRTM |
		 SATA_DWC_INTMR_DMATM);
	out_le32(&hsdev->sata_dwc_regs->errmr, SATA_DWC_SERROR_ERR_BITS);

	dev_dbg(host_pvt.dwc_dev, "%s: INTMR = 0x%08x, ERRMR = 0x%08x\n",
		 __func__, in_le32(&hsdev->sata_dwc_regs->intmr),
		in_le32(&hsdev->sata_dwc_regs->errmr));
}

static void sata_dwc_setup_port(struct ata_ioports *port, unsigned long base)
{
	port->cmd_addr = (void *)base + 0x00;
	port->data_addr = (void *)base + 0x00;

	port->error_addr = (void *)base + 0x04;
	port->feature_addr = (void *)base + 0x04;

	port->nsect_addr = (void *)base + 0x08;

	port->lbal_addr = (void *)base + 0x0c;
	port->lbam_addr = (void *)base + 0x10;
	port->lbah_addr = (void *)base + 0x14;

	port->device_addr = (void *)base + 0x18;
	port->command_addr = (void *)base + 0x1c;
	port->status_addr = (void *)base + 0x1c;

	port->altstatus_addr = (void *)base + 0x20;
	port->ctl_addr = (void *)base + 0x20;
}

static int sata_dwc_port_start(struct ata_port *ap)
{
	int err = 0;
	struct sata_dwc_device *hsdev;
	struct sata_dwc_device_port *hsdevp = NULL;
	struct device *pdev;
	int i;

	hsdev = HSDEV_FROM_AP(ap);

	dev_dbg(ap->dev, "%s: port_no=%d\n", __func__, ap->port_no);

	hsdev->host = ap->host;
	pdev = ap->host->dev;
	if (!pdev) {
		dev_err(ap->dev, "%s: no ap->host->dev\n", __func__);
		err = -ENODEV;
		goto CLEANUP;
	}

	
	hsdevp = kzalloc(sizeof(*hsdevp), GFP_KERNEL);
	if (!hsdevp) {
		dev_err(ap->dev, "%s: kmalloc failed for hsdevp\n", __func__);
		err = -ENOMEM;
		goto CLEANUP;
	}
	hsdevp->hsdev = hsdev;

	for (i = 0; i < SATA_DWC_QCMD_MAX; i++)
		hsdevp->cmd_issued[i] = SATA_DWC_CMD_ISSUED_NOT;

	ap->bmdma_prd = 0;	
	ap->bmdma_prd_dma = 0;

	for (i = 0; i < SATA_DWC_QCMD_MAX; i++) {
		hsdevp->llit[i] = dma_alloc_coherent(pdev,
						     SATA_DWC_DMAC_LLI_TBL_SZ,
						     &(hsdevp->llit_dma[i]),
						     GFP_ATOMIC);
		if (!hsdevp->llit[i]) {
			dev_err(ap->dev, "%s: dma_alloc_coherent failed\n",
				 __func__);
			err = -ENOMEM;
			goto CLEANUP_ALLOC;
		}
	}

	if (ap->port_no == 0)  {
		dev_dbg(ap->dev, "%s: clearing TXCHEN, RXCHEN in DMAC\n",
			__func__);
		out_le32(&hsdev->sata_dwc_regs->dmacr,
			 SATA_DWC_DMACR_TXRXCH_CLEAR);

		dev_dbg(ap->dev, "%s: setting burst size in DBTSR\n",
			 __func__);
		out_le32(&hsdev->sata_dwc_regs->dbtsr,
			 (SATA_DWC_DBTSR_MWR(AHB_DMA_BRST_DFLT) |
			  SATA_DWC_DBTSR_MRD(AHB_DMA_BRST_DFLT)));
	}

	
	clear_serror();
	ap->private_data = hsdevp;
	dev_dbg(ap->dev, "%s: done\n", __func__);
	return 0;

CLEANUP_ALLOC:
	kfree(hsdevp);
CLEANUP:
	dev_dbg(ap->dev, "%s: fail. ap->id = %d\n", __func__, ap->print_id);
	return err;
}

static void sata_dwc_port_stop(struct ata_port *ap)
{
	int i;
	struct sata_dwc_device *hsdev = HSDEV_FROM_AP(ap);
	struct sata_dwc_device_port *hsdevp = HSDEVP_FROM_AP(ap);

	dev_dbg(ap->dev, "%s: ap->id = %d\n", __func__, ap->print_id);

	if (hsdevp && hsdev) {
		
		for (i = 0; i < SATA_DWC_QCMD_MAX; i++) {
			dma_free_coherent(ap->host->dev,
					  SATA_DWC_DMAC_LLI_TBL_SZ,
					 hsdevp->llit[i], hsdevp->llit_dma[i]);
		}

		kfree(hsdevp);
	}
	ap->private_data = NULL;
}

static void sata_dwc_exec_command_by_tag(struct ata_port *ap,
					 struct ata_taskfile *tf,
					 u8 tag, u32 cmd_issued)
{
	unsigned long flags;
	struct sata_dwc_device_port *hsdevp = HSDEVP_FROM_AP(ap);

	dev_dbg(ap->dev, "%s cmd(0x%02x): %s tag=%d\n", __func__, tf->command,
		ata_get_cmd_descript(tf->command), tag);

	spin_lock_irqsave(&ap->host->lock, flags);
	hsdevp->cmd_issued[tag] = cmd_issued;
	spin_unlock_irqrestore(&ap->host->lock, flags);
	clear_serror();
	ata_sff_exec_command(ap, tf);
}

static void sata_dwc_bmdma_setup_by_tag(struct ata_queued_cmd *qc, u8 tag)
{
	sata_dwc_exec_command_by_tag(qc->ap, &qc->tf, tag,
				     SATA_DWC_CMD_ISSUED_PEND);
}

static void sata_dwc_bmdma_setup(struct ata_queued_cmd *qc)
{
	u8 tag = qc->tag;

	if (ata_is_ncq(qc->tf.protocol)) {
		dev_dbg(qc->ap->dev, "%s: ap->link.sactive=0x%08x tag=%d\n",
			__func__, qc->ap->link.sactive, tag);
	} else {
		tag = 0;
	}
	sata_dwc_bmdma_setup_by_tag(qc, tag);
}

static void sata_dwc_bmdma_start_by_tag(struct ata_queued_cmd *qc, u8 tag)
{
	int start_dma;
	u32 reg, dma_chan;
	struct sata_dwc_device *hsdev = HSDEV_FROM_QC(qc);
	struct ata_port *ap = qc->ap;
	struct sata_dwc_device_port *hsdevp = HSDEVP_FROM_AP(ap);
	int dir = qc->dma_dir;
	dma_chan = hsdevp->dma_chan[tag];

	if (hsdevp->cmd_issued[tag] != SATA_DWC_CMD_ISSUED_NOT) {
		start_dma = 1;
		if (dir == DMA_TO_DEVICE)
			hsdevp->dma_pending[tag] = SATA_DWC_DMA_PENDING_TX;
		else
			hsdevp->dma_pending[tag] = SATA_DWC_DMA_PENDING_RX;
	} else {
		dev_err(ap->dev, "%s: Command not pending cmd_issued=%d "
			"(tag=%d) DMA NOT started\n", __func__,
			hsdevp->cmd_issued[tag], tag);
		start_dma = 0;
	}

	dev_dbg(ap->dev, "%s qc=%p tag: %x cmd: 0x%02x dma_dir: %s "
		"start_dma? %x\n", __func__, qc, tag, qc->tf.command,
		get_dma_dir_descript(qc->dma_dir), start_dma);
	sata_dwc_tf_dump(&(qc->tf));

	if (start_dma) {
		reg = core_scr_read(SCR_ERROR);
		if (reg & SATA_DWC_SERROR_ERR_BITS) {
			dev_err(ap->dev, "%s: ****** SError=0x%08x ******\n",
				__func__, reg);
		}

		if (dir == DMA_TO_DEVICE)
			out_le32(&hsdev->sata_dwc_regs->dmacr,
				SATA_DWC_DMACR_TXCHEN);
		else
			out_le32(&hsdev->sata_dwc_regs->dmacr,
				SATA_DWC_DMACR_RXCHEN);

		
		dma_dwc_xfer_start(dma_chan);
	}
}

static void sata_dwc_bmdma_start(struct ata_queued_cmd *qc)
{
	u8 tag = qc->tag;

	if (ata_is_ncq(qc->tf.protocol)) {
		dev_dbg(qc->ap->dev, "%s: ap->link.sactive=0x%08x tag=%d\n",
			__func__, qc->ap->link.sactive, tag);
	} else {
		tag = 0;
	}
	dev_dbg(qc->ap->dev, "%s\n", __func__);
	sata_dwc_bmdma_start_by_tag(qc, tag);
}

static void sata_dwc_qc_prep_by_tag(struct ata_queued_cmd *qc, u8 tag)
{
	struct scatterlist *sg = qc->sg;
	struct ata_port *ap = qc->ap;
	int dma_chan;
	struct sata_dwc_device *hsdev = HSDEV_FROM_AP(ap);
	struct sata_dwc_device_port *hsdevp = HSDEVP_FROM_AP(ap);

	dev_dbg(ap->dev, "%s: port=%d dma dir=%s n_elem=%d\n",
		__func__, ap->port_no, get_dma_dir_descript(qc->dma_dir),
		 qc->n_elem);

	dma_chan = dma_dwc_xfer_setup(sg, qc->n_elem, hsdevp->llit[tag],
				      hsdevp->llit_dma[tag],
				      (void *__iomem)(&hsdev->sata_dwc_regs->\
				      dmadr), qc->dma_dir);
	if (dma_chan < 0) {
		dev_err(ap->dev, "%s: dma_dwc_xfer_setup returns err %d\n",
			__func__, dma_chan);
		return;
	}
	hsdevp->dma_chan[tag] = dma_chan;
}

static unsigned int sata_dwc_qc_issue(struct ata_queued_cmd *qc)
{
	u32 sactive;
	u8 tag = qc->tag;
	struct ata_port *ap = qc->ap;

#ifdef DEBUG_NCQ
	if (qc->tag > 0 || ap->link.sactive > 1)
		dev_info(ap->dev, "%s ap id=%d cmd(0x%02x)=%s qc tag=%d "
			 "prot=%s ap active_tag=0x%08x ap sactive=0x%08x\n",
			 __func__, ap->print_id, qc->tf.command,
			 ata_get_cmd_descript(qc->tf.command),
			 qc->tag, get_prot_descript(qc->tf.protocol),
			 ap->link.active_tag, ap->link.sactive);
#endif

	if (!ata_is_ncq(qc->tf.protocol))
		tag = 0;
	sata_dwc_qc_prep_by_tag(qc, tag);

	if (ata_is_ncq(qc->tf.protocol)) {
		sactive = core_scr_read(SCR_ACTIVE);
		sactive |= (0x00000001 << tag);
		core_scr_write(SCR_ACTIVE, sactive);

		dev_dbg(qc->ap->dev, "%s: tag=%d ap->link.sactive = 0x%08x "
			"sactive=0x%08x\n", __func__, tag, qc->ap->link.sactive,
			sactive);

		ap->ops->sff_tf_load(ap, &qc->tf);
		sata_dwc_exec_command_by_tag(ap, &qc->tf, qc->tag,
					     SATA_DWC_CMD_ISSUED_PEND);
	} else {
		ata_sff_qc_issue(qc);
	}
	return 0;
}


static void sata_dwc_qc_prep(struct ata_queued_cmd *qc)
{
	if ((qc->dma_dir == DMA_NONE) || (qc->tf.protocol == ATA_PROT_PIO))
		return;

#ifdef DEBUG_NCQ
	if (qc->tag > 0)
		dev_info(qc->ap->dev, "%s: qc->tag=%d ap->active_tag=0x%08x\n",
			 __func__, qc->tag, qc->ap->link.active_tag);

	return ;
#endif
}

static void sata_dwc_error_handler(struct ata_port *ap)
{
	ap->link.flags |= ATA_LFLAG_NO_HRST;
	ata_sff_error_handler(ap);
}

static struct scsi_host_template sata_dwc_sht = {
	ATA_NCQ_SHT(DRV_NAME),
	.sg_tablesize		= LIBATA_MAX_PRD,
	.can_queue		= ATA_DEF_QUEUE,	
	.dma_boundary		= ATA_DMA_BOUNDARY,
};

static struct ata_port_operations sata_dwc_ops = {
	.inherits		= &ata_sff_port_ops,

	.error_handler		= sata_dwc_error_handler,

	.qc_prep		= sata_dwc_qc_prep,
	.qc_issue		= sata_dwc_qc_issue,

	.scr_read		= sata_dwc_scr_read,
	.scr_write		= sata_dwc_scr_write,

	.port_start		= sata_dwc_port_start,
	.port_stop		= sata_dwc_port_stop,

	.bmdma_setup		= sata_dwc_bmdma_setup,
	.bmdma_start		= sata_dwc_bmdma_start,
};

static const struct ata_port_info sata_dwc_port_info[] = {
	{
		.flags		= ATA_FLAG_SATA | ATA_FLAG_NCQ,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &sata_dwc_ops,
	},
};

static int sata_dwc_probe(struct platform_device *ofdev)
{
	struct sata_dwc_device *hsdev;
	u32 idr, versionr;
	char *ver = (char *)&versionr;
	u8 *base = NULL;
	int err = 0;
	int irq, rc;
	struct ata_host *host;
	struct ata_port_info pi = sata_dwc_port_info[0];
	const struct ata_port_info *ppi[] = { &pi, NULL };

	
	hsdev = kzalloc(sizeof(*hsdev), GFP_KERNEL);
	if (hsdev == NULL) {
		dev_err(&ofdev->dev, "kmalloc failed for hsdev\n");
		err = -ENOMEM;
		goto error;
	}

	
	base = of_iomap(ofdev->dev.of_node, 0);
	if (!base) {
		dev_err(&ofdev->dev, "ioremap failed for SATA register"
			" address\n");
		err = -ENODEV;
		goto error_kmalloc;
	}
	hsdev->reg_base = base;
	dev_dbg(&ofdev->dev, "ioremap done for SATA register address\n");

	
	hsdev->sata_dwc_regs = (void *__iomem)(base + SATA_DWC_REG_OFFSET);

	
	host = ata_host_alloc_pinfo(&ofdev->dev, ppi, SATA_DWC_MAX_PORTS);
	if (!host) {
		dev_err(&ofdev->dev, "ata_host_alloc_pinfo failed\n");
		err = -ENOMEM;
		goto error_iomap;
	}

	host->private_data = hsdev;

	
	host->ports[0]->ioaddr.cmd_addr = base;
	host->ports[0]->ioaddr.scr_addr = base + SATA_DWC_SCR_OFFSET;
	host_pvt.scr_addr_sstatus = base + SATA_DWC_SCR_OFFSET;
	sata_dwc_setup_port(&host->ports[0]->ioaddr, (unsigned long)base);

	
	idr = in_le32(&hsdev->sata_dwc_regs->idr);
	versionr = in_le32(&hsdev->sata_dwc_regs->versionr);
	dev_notice(&ofdev->dev, "id %d, controller version %c.%c%c\n",
		   idr, ver[0], ver[1], ver[2]);

	
	irq = irq_of_parse_and_map(ofdev->dev.of_node, 1);
	if (irq == NO_IRQ) {
		dev_err(&ofdev->dev, "no SATA DMA irq\n");
		err = -ENODEV;
		goto error_out;
	}

	
	host_pvt.sata_dma_regs = of_iomap(ofdev->dev.of_node, 1);
	if (!(host_pvt.sata_dma_regs)) {
		dev_err(&ofdev->dev, "ioremap failed for AHBDMA register"
			" address\n");
		err = -ENODEV;
		goto error_out;
	}

	
	host_pvt.dwc_dev = &ofdev->dev;

	
	dma_dwc_init(hsdev, irq);

	
	sata_dwc_enable_interrupts(hsdev);

	
	irq = irq_of_parse_and_map(ofdev->dev.of_node, 0);
	if (irq == NO_IRQ) {
		dev_err(&ofdev->dev, "no SATA DMA irq\n");
		err = -ENODEV;
		goto error_out;
	}

	rc = ata_host_activate(host, irq, sata_dwc_isr, 0, &sata_dwc_sht);

	if (rc != 0)
		dev_err(&ofdev->dev, "failed to activate host");

	dev_set_drvdata(&ofdev->dev, host);
	return 0;

error_out:
	
	dma_dwc_exit(hsdev);

error_iomap:
	iounmap(base);
error_kmalloc:
	kfree(hsdev);
error:
	return err;
}

static int sata_dwc_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct ata_host *host = dev_get_drvdata(dev);
	struct sata_dwc_device *hsdev = host->private_data;

	ata_host_detach(host);
	dev_set_drvdata(dev, NULL);

	
	dma_dwc_exit(hsdev);

	iounmap(hsdev->reg_base);
	kfree(hsdev);
	kfree(host);
	dev_dbg(&ofdev->dev, "done\n");
	return 0;
}

static const struct of_device_id sata_dwc_match[] = {
	{ .compatible = "amcc,sata-460ex", },
	{}
};
MODULE_DEVICE_TABLE(of, sata_dwc_match);

static struct platform_driver sata_dwc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sata_dwc_match,
	},
	.probe = sata_dwc_probe,
	.remove = sata_dwc_remove,
};

module_platform_driver(sata_dwc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mark Miesfeld <mmiesfeld@amcc.com>");
MODULE_DESCRIPTION("DesignWare Cores SATA controller low lever driver");
MODULE_VERSION(DRV_VERSION);
