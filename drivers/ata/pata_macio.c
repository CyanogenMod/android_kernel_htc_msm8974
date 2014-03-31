/*
 * Libata based driver for Apple "macio" family of PATA controllers
 *
 * Copyright 2008/2009 Benjamin Herrenschmidt, IBM Corp
 *                     <benh@kernel.crashing.org>
 *
 * Some bits and pieces from drivers/ide/ppc/pmac.c
 *
 */

#undef DEBUG
#undef DEBUG_DMA

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/ata.h>
#include <linux/libata.h>
#include <linux/adb.h>
#include <linux/pmu.h>
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/gfp.h>

#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>

#include <asm/macio.h>
#include <asm/io.h>
#include <asm/dbdma.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/mediabay.h>

#ifdef DEBUG_DMA
#define dev_dbgdma(dev, format, arg...)		\
	dev_printk(KERN_DEBUG , dev , format , ## arg)
#else
#define dev_dbgdma(dev, format, arg...)		\
	({ if (0) dev_printk(KERN_DEBUG, dev, format, ##arg); 0; })
#endif

#define DRV_NAME	"pata_macio"
#define DRV_VERSION	"0.9"

enum {
	controller_ohare,	
	controller_heathrow,	
	controller_kl_ata3,	
	controller_kl_ata4,	
	controller_un_ata6,	
	controller_k2_ata6,	
	controller_sh_ata6,	
};

static const char* macio_ata_names[] = {
	"OHare ATA",		
	"Heathrow ATA",		
	"KeyLargo ATA-3",	
	"KeyLargo ATA-4",	
	"UniNorth ATA-6",	
	"K2 ATA-6",		
	"Shasta ATA-6",		
};

#define IDE_TIMING_CONFIG	0x200
#define IDE_INTERRUPT		0x300

#define IDE_KAUAI_PIO_CONFIG	0x200
#define IDE_KAUAI_ULTRA_CONFIG	0x210
#define IDE_KAUAI_POLL_CONFIG	0x220


#define SYSCLK_TICKS(t)		(((t) + IDE_SYSCLK_NS - 1) / IDE_SYSCLK_NS)
#define SYSCLK_TICKS_66(t)	(((t) + IDE_SYSCLK_66_NS - 1) / IDE_SYSCLK_66_NS)
#define IDE_SYSCLK_NS		30	
#define IDE_SYSCLK_66_NS	15	

#define TR_133_PIOREG_PIO_MASK		0xff000fff
#define TR_133_PIOREG_MDMA_MASK		0x00fff800
#define TR_133_UDMAREG_UDMA_MASK	0x0003ffff
#define TR_133_UDMAREG_UDMA_EN		0x00000001

#define TR_100_PIO_ADDRSETUP_MASK	0xff000000 
#define TR_100_PIO_ADDRSETUP_SHIFT	24
#define TR_100_MDMA_MASK		0x00fff000
#define TR_100_MDMA_RECOVERY_MASK	0x00fc0000
#define TR_100_MDMA_RECOVERY_SHIFT	18
#define TR_100_MDMA_ACCESS_MASK		0x0003f000
#define TR_100_MDMA_ACCESS_SHIFT	12
#define TR_100_PIO_MASK			0xff000fff
#define TR_100_PIO_RECOVERY_MASK	0x00000fc0
#define TR_100_PIO_RECOVERY_SHIFT	6
#define TR_100_PIO_ACCESS_MASK		0x0000003f
#define TR_100_PIO_ACCESS_SHIFT		0

#define TR_100_UDMAREG_UDMA_MASK	0x0000ffff
#define TR_100_UDMAREG_UDMA_EN		0x00000001


#define TR_66_UDMA_MASK			0xfff00000
#define TR_66_UDMA_EN			0x00100000 
#define TR_66_PIO_ADDRSETUP_MASK	0xe0000000 
#define TR_66_PIO_ADDRSETUP_SHIFT	29
#define TR_66_UDMA_RDY2PAUS_MASK	0x1e000000 
#define TR_66_UDMA_RDY2PAUS_SHIFT	25
#define TR_66_UDMA_WRDATASETUP_MASK	0x01e00000 
#define TR_66_UDMA_WRDATASETUP_SHIFT	21
#define TR_66_MDMA_MASK			0x000ffc00
#define TR_66_MDMA_RECOVERY_MASK	0x000f8000
#define TR_66_MDMA_RECOVERY_SHIFT	15
#define TR_66_MDMA_ACCESS_MASK		0x00007c00
#define TR_66_MDMA_ACCESS_SHIFT		10
#define TR_66_PIO_MASK			0xe00003ff
#define TR_66_PIO_RECOVERY_MASK		0x000003e0
#define TR_66_PIO_RECOVERY_SHIFT	5
#define TR_66_PIO_ACCESS_MASK		0x0000001f
#define TR_66_PIO_ACCESS_SHIFT		0

#define TR_33_MDMA_MASK			0x003ff800
#define TR_33_MDMA_RECOVERY_MASK	0x001f0000
#define TR_33_MDMA_RECOVERY_SHIFT	16
#define TR_33_MDMA_ACCESS_MASK		0x0000f800
#define TR_33_MDMA_ACCESS_SHIFT		11
#define TR_33_MDMA_HALFTICK		0x00200000
#define TR_33_PIO_MASK			0x000007ff
#define TR_33_PIO_E			0x00000400
#define TR_33_PIO_RECOVERY_MASK		0x000003e0
#define TR_33_PIO_RECOVERY_SHIFT	5
#define TR_33_PIO_ACCESS_MASK		0x0000001f
#define TR_33_PIO_ACCESS_SHIFT		0

#define IDE_INTR_DMA			0x80000000
#define IDE_INTR_DEVICE			0x40000000

#define KAUAI_FCR_UATA_MAGIC		0x00000004
#define KAUAI_FCR_UATA_RESET_N		0x00000002
#define KAUAI_FCR_UATA_ENABLE		0x00000001


#define MAX_DCMDS		256

#define MAX_DBDMA_SEG		0xff00


#define IDE_WAKEUP_DELAY_MS	1000

struct pata_macio_timing;

struct pata_macio_priv {
	int				kind;
	int				aapl_bus_id;
	int				mediabay : 1;
	struct device_node		*node;
	struct macio_dev		*mdev;
	struct pci_dev			*pdev;
	struct device			*dev;
	int				irq;
	u32				treg[2][2];
	void __iomem			*tfregs;
	void __iomem			*kauai_fcr;
	struct dbdma_cmd *		dma_table_cpu;
	dma_addr_t			dma_table_dma;
	struct ata_host			*host;
	const struct pata_macio_timing	*timings;
};

struct pata_macio_timing {
	int	mode;
	u32	reg1;	
	u32	reg2;	
};

static const struct pata_macio_timing pata_macio_ohare_timings[] = {
	{ XFER_PIO_0,		0x00000526,	0, },
	{ XFER_PIO_1,		0x00000085,	0, },
	{ XFER_PIO_2,		0x00000025,	0, },
	{ XFER_PIO_3,		0x00000025,	0, },
	{ XFER_PIO_4,		0x00000025,	0, },
	{ XFER_MW_DMA_0,	0x00074000,	0, },
	{ XFER_MW_DMA_1,	0x00221000,	0, },
	{ XFER_MW_DMA_2,	0x00211000,	0, },
	{ -1, 0, 0 }
};

static const struct pata_macio_timing pata_macio_heathrow_timings[] = {
	{ XFER_PIO_0,		0x00000526,	0, },
	{ XFER_PIO_1,		0x00000085,	0, },
	{ XFER_PIO_2,		0x00000025,	0, },
	{ XFER_PIO_3,		0x00000025,	0, },
	{ XFER_PIO_4,		0x00000025,	0, },
	{ XFER_MW_DMA_0,	0x00074000,	0, },
	{ XFER_MW_DMA_1,	0x00221000,	0, },
	{ XFER_MW_DMA_2,	0x00211000,	0, },
	{ -1, 0, 0 }
};

static const struct pata_macio_timing pata_macio_kl33_timings[] = {
	{ XFER_PIO_0,		0x00000526,	0, },
	{ XFER_PIO_1,		0x00000085,	0, },
	{ XFER_PIO_2,		0x00000025,	0, },
	{ XFER_PIO_3,		0x00000025,	0, },
	{ XFER_PIO_4,		0x00000025,	0, },
	{ XFER_MW_DMA_0,	0x00084000,	0, },
	{ XFER_MW_DMA_1,	0x00021800,	0, },
	{ XFER_MW_DMA_2,	0x00011800,	0, },
	{ -1, 0, 0 }
};

static const struct pata_macio_timing pata_macio_kl66_timings[] = {
	{ XFER_PIO_0,		0x0000038c,	0, },
	{ XFER_PIO_1,		0x0000020a,	0, },
	{ XFER_PIO_2,		0x00000127,	0, },
	{ XFER_PIO_3,		0x000000c6,	0, },
	{ XFER_PIO_4,		0x00000065,	0, },
	{ XFER_MW_DMA_0,	0x00084000,	0, },
	{ XFER_MW_DMA_1,	0x00029800,	0, },
	{ XFER_MW_DMA_2,	0x00019400,	0, },
	{ XFER_UDMA_0,		0x19100000,	0, },
	{ XFER_UDMA_1,		0x14d00000,	0, },
	{ XFER_UDMA_2,		0x10900000,	0, },
	{ XFER_UDMA_3,		0x0c700000,	0, },
	{ XFER_UDMA_4,		0x0c500000,	0, },
	{ -1, 0, 0 }
};

static const struct pata_macio_timing pata_macio_kauai_timings[] = {
	{ XFER_PIO_0,		0x08000a92,	0, },
	{ XFER_PIO_1,		0x0800060f,	0, },
	{ XFER_PIO_2,		0x0800038b,	0, },
	{ XFER_PIO_3,		0x05000249,	0, },
	{ XFER_PIO_4,		0x04000148,	0, },
	{ XFER_MW_DMA_0,	0x00618000,	0, },
	{ XFER_MW_DMA_1,	0x00209000,	0, },
	{ XFER_MW_DMA_2,	0x00148000,	0, },
	{ XFER_UDMA_0,		         0,	0x000070c1, },
	{ XFER_UDMA_1,		         0,	0x00005d81, },
	{ XFER_UDMA_2,		         0,	0x00004a61, },
	{ XFER_UDMA_3,		         0,	0x00003a51, },
	{ XFER_UDMA_4,		         0,	0x00002a31, },
	{ XFER_UDMA_5,		         0,	0x00002921, },
	{ -1, 0, 0 }
};

static const struct pata_macio_timing pata_macio_shasta_timings[] = {
	{ XFER_PIO_0,		0x0a000c97,	0, },
	{ XFER_PIO_1,		0x07000712,	0, },
	{ XFER_PIO_2,		0x040003cd,	0, },
	{ XFER_PIO_3,		0x0500028b,	0, },
	{ XFER_PIO_4,		0x0400010a,	0, },
	{ XFER_MW_DMA_0,	0x00820800,	0, },
	{ XFER_MW_DMA_1,	0x0028b000,	0, },
	{ XFER_MW_DMA_2,	0x001ca000,	0, },
	{ XFER_UDMA_0,		         0,	0x00035901, },
	{ XFER_UDMA_1,		         0,	0x000348b1, },
	{ XFER_UDMA_2,		         0,	0x00033881, },
	{ XFER_UDMA_3,		         0,	0x00033861, },
	{ XFER_UDMA_4,		         0,	0x00033841, },
	{ XFER_UDMA_5,		         0,	0x00033031, },
	{ XFER_UDMA_6,		         0,	0x00033021, },
	{ -1, 0, 0 }
};

static const struct pata_macio_timing *pata_macio_find_timing(
					    struct pata_macio_priv *priv,
					    int mode)
{
	int i;

	for (i = 0; priv->timings[i].mode > 0; i++) {
		if (priv->timings[i].mode == mode)
			return &priv->timings[i];
	}
	return NULL;
}


static void pata_macio_apply_timings(struct ata_port *ap, unsigned int device)
{
	struct pata_macio_priv *priv = ap->private_data;
	void __iomem *rbase = ap->ioaddr.cmd_addr;

	if (priv->kind == controller_sh_ata6 ||
	    priv->kind == controller_un_ata6 ||
	    priv->kind == controller_k2_ata6) {
		writel(priv->treg[device][0], rbase + IDE_KAUAI_PIO_CONFIG);
		writel(priv->treg[device][1], rbase + IDE_KAUAI_ULTRA_CONFIG);
	} else
		writel(priv->treg[device][0], rbase + IDE_TIMING_CONFIG);
}

static void pata_macio_dev_select(struct ata_port *ap, unsigned int device)
{
	ata_sff_dev_select(ap, device);

	
	pata_macio_apply_timings(ap, device);
}

static void pata_macio_set_timings(struct ata_port *ap,
				   struct ata_device *adev)
{
	struct pata_macio_priv *priv = ap->private_data;
	const struct pata_macio_timing *t;

	dev_dbg(priv->dev, "Set timings: DEV=%d,PIO=0x%x (%s),DMA=0x%x (%s)\n",
		adev->devno,
		adev->pio_mode,
		ata_mode_string(ata_xfer_mode2mask(adev->pio_mode)),
		adev->dma_mode,
		ata_mode_string(ata_xfer_mode2mask(adev->dma_mode)));

	
	priv->treg[adev->devno][0] = priv->treg[adev->devno][1] = 0;

	
	t = pata_macio_find_timing(priv, adev->pio_mode);
	if (t == NULL) {
		dev_warn(priv->dev, "Invalid PIO timing requested: 0x%x\n",
			 adev->pio_mode);
		t = pata_macio_find_timing(priv, XFER_PIO_0);
	}
	BUG_ON(t == NULL);

	
	priv->treg[adev->devno][0] |= t->reg1;

	
	t = pata_macio_find_timing(priv, adev->dma_mode);
	if (t == NULL || (t->reg1 == 0 && t->reg2 == 0)) {
		dev_dbg(priv->dev, "DMA timing not set yet, using MW_DMA_0\n");
		t = pata_macio_find_timing(priv, XFER_MW_DMA_0);
	}
	BUG_ON(t == NULL);

	
	priv->treg[adev->devno][0] |= t->reg1;
	priv->treg[adev->devno][1] |= t->reg2;

	dev_dbg(priv->dev, " -> %08x %08x\n",
		priv->treg[adev->devno][0],
		priv->treg[adev->devno][1]);

	
	pata_macio_apply_timings(ap, adev->devno);
}

static void pata_macio_default_timings(struct pata_macio_priv *priv)
{
	unsigned int value, value2 = 0;

	switch(priv->kind) {
		case controller_sh_ata6:
			value = 0x0a820c97;
			value2 = 0x00033031;
			break;
		case controller_un_ata6:
		case controller_k2_ata6:
			value = 0x08618a92;
			value2 = 0x00002921;
			break;
		case controller_kl_ata4:
			value = 0x0008438c;
			break;
		case controller_kl_ata3:
			value = 0x00084526;
			break;
		case controller_heathrow:
		case controller_ohare:
		default:
			value = 0x00074526;
			break;
	}
	priv->treg[0][0] = priv->treg[1][0] = value;
	priv->treg[0][1] = priv->treg[1][1] = value2;
}

static int pata_macio_cable_detect(struct ata_port *ap)
{
	struct pata_macio_priv *priv = ap->private_data;

	
	if (priv->kind == controller_kl_ata4 ||
	    priv->kind == controller_un_ata6 ||
	    priv->kind == controller_k2_ata6 ||
	    priv->kind == controller_sh_ata6) {
		const char* cable = of_get_property(priv->node, "cable-type",
						    NULL);
		struct device_node *root = of_find_node_by_path("/");
		const char *model = of_get_property(root, "model", NULL);

		if (cable && !strncmp(cable, "80-", 3)) {
			if (!strncmp(model, "PowerBook", 9))
				return ATA_CBL_PATA40_SHORT;
			else
				return ATA_CBL_PATA80;
		}
	}

	if (of_device_is_compatible(priv->node, "K2-UATA") ||
	    of_device_is_compatible(priv->node, "shasta-ata"))
		return ATA_CBL_PATA80;

	
	return ATA_CBL_PATA40;
}

static void pata_macio_qc_prep(struct ata_queued_cmd *qc)
{
	unsigned int write = (qc->tf.flags & ATA_TFLAG_WRITE);
	struct ata_port *ap = qc->ap;
	struct pata_macio_priv *priv = ap->private_data;
	struct scatterlist *sg;
	struct dbdma_cmd *table;
	unsigned int si, pi;

	dev_dbgdma(priv->dev, "%s: qc %p flags %lx, write %d dev %d\n",
		   __func__, qc, qc->flags, write, qc->dev->devno);

	if (!(qc->flags & ATA_QCFLAG_DMAMAP))
		return;

	table = (struct dbdma_cmd *) priv->dma_table_cpu;

	pi = 0;
	for_each_sg(qc->sg, sg, qc->n_elem, si) {
		u32 addr, sg_len, len;

		addr = (u32) sg_dma_address(sg);
		sg_len = sg_dma_len(sg);

		while (sg_len) {
			
			BUG_ON (pi++ >= MAX_DCMDS);

			len = (sg_len < MAX_DBDMA_SEG) ? sg_len : MAX_DBDMA_SEG;
			st_le16(&table->command, write ? OUTPUT_MORE: INPUT_MORE);
			st_le16(&table->req_count, len);
			st_le32(&table->phy_addr, addr);
			table->cmd_dep = 0;
			table->xfer_status = 0;
			table->res_count = 0;
			addr += len;
			sg_len -= len;
			++table;
		}
	}

	
	BUG_ON(!pi);

	
	table--;
	st_le16(&table->command, write ? OUTPUT_LAST: INPUT_LAST);
	table++;

	
	memset(table, 0, sizeof(struct dbdma_cmd));
	st_le16(&table->command, DBDMA_STOP);

	dev_dbgdma(priv->dev, "%s: %d DMA list entries\n", __func__, pi);
}


static void pata_macio_freeze(struct ata_port *ap)
{
	struct dbdma_regs __iomem *dma_regs = ap->ioaddr.bmdma_addr;

	if (dma_regs) {
		unsigned int timeout = 1000000;

		
		writel((RUN|PAUSE|FLUSH|WAKE|DEAD) << 16, &dma_regs->control);
		while (--timeout && (readl(&dma_regs->status) & RUN))
			udelay(1);
	}

	ata_sff_freeze(ap);
}


static void pata_macio_bmdma_setup(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pata_macio_priv *priv = ap->private_data;
	struct dbdma_regs __iomem *dma_regs = ap->ioaddr.bmdma_addr;
	int dev = qc->dev->devno;

	dev_dbgdma(priv->dev, "%s: qc %p\n", __func__, qc);

	
	writel(priv->dma_table_dma, &dma_regs->cmdptr);

	if (priv->kind == controller_kl_ata4 &&
	    (priv->treg[dev][0] & TR_66_UDMA_EN)) {
		void __iomem *rbase = ap->ioaddr.cmd_addr;
		u32 reg = priv->treg[dev][0];

		if (!(qc->tf.flags & ATA_TFLAG_WRITE))
			reg += 0x00800000;
		writel(reg, rbase + IDE_TIMING_CONFIG);
	}

	
	ap->ops->sff_exec_command(ap, &qc->tf);
}

static void pata_macio_bmdma_start(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pata_macio_priv *priv = ap->private_data;
	struct dbdma_regs __iomem *dma_regs = ap->ioaddr.bmdma_addr;

	dev_dbgdma(priv->dev, "%s: qc %p\n", __func__, qc);

	writel((RUN << 16) | RUN, &dma_regs->control);
	
	(void)readl(&dma_regs->control);
}

static void pata_macio_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pata_macio_priv *priv = ap->private_data;
	struct dbdma_regs __iomem *dma_regs = ap->ioaddr.bmdma_addr;
	unsigned int timeout = 1000000;

	dev_dbgdma(priv->dev, "%s: qc %p\n", __func__, qc);

	
	writel (((RUN|WAKE|DEAD) << 16), &dma_regs->control);
	while (--timeout && (readl(&dma_regs->status) & RUN))
		udelay(1);
}

static u8 pata_macio_bmdma_status(struct ata_port *ap)
{
	struct pata_macio_priv *priv = ap->private_data;
	struct dbdma_regs __iomem *dma_regs = ap->ioaddr.bmdma_addr;
	u32 dstat, rstat = ATA_DMA_INTR;
	unsigned long timeout = 0;

	dstat = readl(&dma_regs->status);

	dev_dbgdma(priv->dev, "%s: dstat=%x\n", __func__, dstat);


	
	if ((dstat & (RUN|DEAD)) != RUN)
		rstat |= ATA_DMA_ERR;

	if ((dstat & ACTIVE) == 0)
		return rstat;

	dev_dbgdma(priv->dev, "%s: DMA still active, flushing...\n", __func__);

	udelay(1);
	writel((FLUSH << 16) | FLUSH, &dma_regs->control);
	for (;;) {
		udelay(1);
		dstat = readl(&dma_regs->status);
		if ((dstat & FLUSH) == 0)
			break;
		if (++timeout > 1000) {
			dev_warn(priv->dev, "timeout flushing DMA\n");
			rstat |= ATA_DMA_ERR;
			break;
		}
	}
	return rstat;
}

static int pata_macio_port_start(struct ata_port *ap)
{
	struct pata_macio_priv *priv = ap->private_data;

	if (ap->ioaddr.bmdma_addr == NULL)
		return 0;

	priv->dma_table_cpu =
		dmam_alloc_coherent(priv->dev,
				    (MAX_DCMDS + 2) * sizeof(struct dbdma_cmd),
				    &priv->dma_table_dma, GFP_KERNEL);
	if (priv->dma_table_cpu == NULL) {
		dev_err(priv->dev, "Unable to allocate DMA command list\n");
		ap->ioaddr.bmdma_addr = NULL;
		ap->mwdma_mask = 0;
		ap->udma_mask = 0;
	}
	return 0;
}

static void pata_macio_irq_clear(struct ata_port *ap)
{
	struct pata_macio_priv *priv = ap->private_data;

	

	dev_dbgdma(priv->dev, "%s\n", __func__);
}

static void pata_macio_reset_hw(struct pata_macio_priv *priv, int resume)
{
	dev_dbg(priv->dev, "Enabling & resetting... \n");

	if (priv->mediabay)
		return;

	if (priv->kind == controller_ohare && !resume) {
		ppc_md.feature_call(PMAC_FTR_IDE_ENABLE, priv->node, 0, 1);
	} else {
		int rc;

 		
		rc = ppc_md.feature_call(PMAC_FTR_IDE_RESET,
					 priv->node, priv->aapl_bus_id, 1);
		ppc_md.feature_call(PMAC_FTR_IDE_ENABLE,
				    priv->node, priv->aapl_bus_id, 1);
		msleep(10);
		
		if (rc == 0) {
			ppc_md.feature_call(PMAC_FTR_IDE_RESET,
					    priv->node, priv->aapl_bus_id, 0);
			msleep(IDE_WAKEUP_DELAY_MS);
		}
	}

	
	if (priv->pdev && resume) {
		int rc;

		pci_restore_state(priv->pdev);
		rc = pcim_enable_device(priv->pdev);
		if (rc)
			dev_err(&priv->pdev->dev,
				"Failed to enable device after resume (%d)\n",
				rc);
		else
			pci_set_master(priv->pdev);
	}

	if (priv->kauai_fcr)
		writel(KAUAI_FCR_UATA_MAGIC |
		       KAUAI_FCR_UATA_RESET_N |
		       KAUAI_FCR_UATA_ENABLE, priv->kauai_fcr);
}

static int pata_macio_slave_config(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct pata_macio_priv *priv = ap->private_data;
	struct ata_device *dev;
	u16 cmd;
	int rc;

	
	rc = ata_scsi_slave_config(sdev);
	if (rc)
		return rc;

	
	dev = &ap->link.device[sdev->id];

	
	if (priv->kind == controller_ohare) {
		blk_queue_update_dma_alignment(sdev->request_queue, 31);
		blk_queue_update_dma_pad(sdev->request_queue, 31);

		
		ata_dev_info(dev, "OHare alignment limits applied\n");
		return 0;
	}

	
	if (dev->class != ATA_DEV_ATAPI)
		return 0;

	
	if (priv->kind == controller_sh_ata6 || priv->kind == controller_k2_ata6) {
		
		blk_queue_update_dma_alignment(sdev->request_queue, 15);
		blk_queue_update_dma_pad(sdev->request_queue, 15);

		BUG_ON(!priv->pdev);
		pci_write_config_byte(priv->pdev, PCI_CACHE_LINE_SIZE, 0x08);
		pci_read_config_word(priv->pdev, PCI_COMMAND, &cmd);
		pci_write_config_word(priv->pdev, PCI_COMMAND,
				      cmd | PCI_COMMAND_INVALIDATE);

		
		ata_dev_info(dev, "K2/Shasta alignment limits applied\n");
	}

	return 0;
}

#ifdef CONFIG_PM

static int pata_macio_do_suspend(struct pata_macio_priv *priv, pm_message_t mesg)
{
	int rc;

	
	rc = ata_host_suspend(priv->host, mesg);
	if (rc)
		return rc;

	
	pata_macio_default_timings(priv);

	disable_irq(priv->irq);

	
	if (priv->mediabay)
		return 0;

	
	if (priv->kauai_fcr) {
		u32 fcr = readl(priv->kauai_fcr);
		fcr &= ~(KAUAI_FCR_UATA_RESET_N | KAUAI_FCR_UATA_ENABLE);
		writel(fcr, priv->kauai_fcr);
	}

	if (priv->pdev) {
		pci_save_state(priv->pdev);
		pci_disable_device(priv->pdev);
	}

	
	ppc_md.feature_call(PMAC_FTR_IDE_ENABLE, priv->node,
			    priv->aapl_bus_id, 0);

	return 0;
}

static int pata_macio_do_resume(struct pata_macio_priv *priv)
{
	
	pata_macio_reset_hw(priv, 1);

	
	pata_macio_apply_timings(priv->host->ports[0], 0);

	
	enable_irq(priv->irq);

	
	ata_host_resume(priv->host);

	return 0;
}

#endif 

static struct scsi_host_template pata_macio_sht = {
	ATA_BASE_SHT(DRV_NAME),
	.sg_tablesize		= MAX_DCMDS,
	
	.dma_boundary		= ATA_DMA_BOUNDARY,
	.slave_configure	= pata_macio_slave_config,
};

static struct ata_port_operations pata_macio_ops = {
	.inherits		= &ata_bmdma_port_ops,

	.freeze			= pata_macio_freeze,
	.set_piomode		= pata_macio_set_timings,
	.set_dmamode		= pata_macio_set_timings,
	.cable_detect		= pata_macio_cable_detect,
	.sff_dev_select		= pata_macio_dev_select,
	.qc_prep		= pata_macio_qc_prep,
	.bmdma_setup		= pata_macio_bmdma_setup,
	.bmdma_start		= pata_macio_bmdma_start,
	.bmdma_stop		= pata_macio_bmdma_stop,
	.bmdma_status		= pata_macio_bmdma_status,
	.port_start		= pata_macio_port_start,
	.sff_irq_clear		= pata_macio_irq_clear,
};

static void __devinit pata_macio_invariants(struct pata_macio_priv *priv)
{
	const int *bidp;

	
	if (of_device_is_compatible(priv->node, "shasta-ata")) {
		priv->kind = controller_sh_ata6;
	        priv->timings = pata_macio_shasta_timings;
	} else if (of_device_is_compatible(priv->node, "kauai-ata")) {
		priv->kind = controller_un_ata6;
	        priv->timings = pata_macio_kauai_timings;
	} else if (of_device_is_compatible(priv->node, "K2-UATA")) {
		priv->kind = controller_k2_ata6;
	        priv->timings = pata_macio_kauai_timings;
	} else if (of_device_is_compatible(priv->node, "keylargo-ata")) {
		if (strcmp(priv->node->name, "ata-4") == 0) {
			priv->kind = controller_kl_ata4;
			priv->timings = pata_macio_kl66_timings;
		} else {
			priv->kind = controller_kl_ata3;
			priv->timings = pata_macio_kl33_timings;
		}
	} else if (of_device_is_compatible(priv->node, "heathrow-ata")) {
		priv->kind = controller_heathrow;
		priv->timings = pata_macio_heathrow_timings;
	} else {
		priv->kind = controller_ohare;
		priv->timings = pata_macio_ohare_timings;
	}

	

	
	bidp = of_get_property(priv->node, "AAPL,bus-id", NULL);
	priv->aapl_bus_id =  bidp ? *bidp : 0;

	
	if (priv->mediabay && bidp == 0)
		priv->aapl_bus_id = 1;
}

static void __devinit pata_macio_setup_ios(struct ata_ioports *ioaddr,
					   void __iomem * base,
					   void __iomem * dma)
{
	
	ioaddr->cmd_addr	= base;

	
	ioaddr->data_addr	= base + (ATA_REG_DATA    << 4);
	ioaddr->error_addr	= base + (ATA_REG_ERR     << 4);
	ioaddr->feature_addr	= base + (ATA_REG_FEATURE << 4);
	ioaddr->nsect_addr	= base + (ATA_REG_NSECT   << 4);
	ioaddr->lbal_addr	= base + (ATA_REG_LBAL    << 4);
	ioaddr->lbam_addr	= base + (ATA_REG_LBAM    << 4);
	ioaddr->lbah_addr	= base + (ATA_REG_LBAH    << 4);
	ioaddr->device_addr	= base + (ATA_REG_DEVICE  << 4);
	ioaddr->status_addr	= base + (ATA_REG_STATUS  << 4);
	ioaddr->command_addr	= base + (ATA_REG_CMD     << 4);
	ioaddr->altstatus_addr	= base + 0x160;
	ioaddr->ctl_addr	= base + 0x160;
	ioaddr->bmdma_addr	= dma;
}

static void __devinit pmac_macio_calc_timing_masks(struct pata_macio_priv *priv,
						   struct ata_port_info   *pinfo)
{
	int i = 0;

	pinfo->pio_mask		= 0;
	pinfo->mwdma_mask	= 0;
	pinfo->udma_mask	= 0;

	while (priv->timings[i].mode > 0) {
		unsigned int mask = 1U << (priv->timings[i].mode & 0x0f);
		switch(priv->timings[i].mode & 0xf0) {
		case 0x00: 
			pinfo->pio_mask |= (mask >> 8);
			break;
		case 0x20: 
			pinfo->mwdma_mask |= mask;
			break;
		case 0x40: 
			pinfo->udma_mask |= mask;
			break;
		}
		i++;
	}
	dev_dbg(priv->dev, "Supported masks: PIO=%lx, MWDMA=%lx, UDMA=%lx\n",
		pinfo->pio_mask, pinfo->mwdma_mask, pinfo->udma_mask);
}

static int __devinit pata_macio_common_init(struct pata_macio_priv	*priv,
					    resource_size_t		tfregs,
					    resource_size_t		dmaregs,
					    resource_size_t		fcregs,
					    unsigned long		irq)
{
	struct ata_port_info		pinfo;
	const struct ata_port_info	*ppi[] = { &pinfo, NULL };
	void __iomem			*dma_regs = NULL;

	pata_macio_invariants(priv);

	
	pata_macio_default_timings(priv);

	dma_set_max_seg_size(priv->dev, MAX_DBDMA_SEG);

	
	memset(&pinfo, 0, sizeof(struct ata_port_info));
	pmac_macio_calc_timing_masks(priv, &pinfo);
	pinfo.flags		= ATA_FLAG_SLAVE_POSS;
	pinfo.port_ops		= &pata_macio_ops;
	pinfo.private_data	= priv;

	priv->host = ata_host_alloc_pinfo(priv->dev, ppi, 1);
	if (priv->host == NULL) {
		dev_err(priv->dev, "Failed to allocate ATA port structure\n");
		return -ENOMEM;
	}

	
	priv->host->private_data = priv;

	
	priv->tfregs = devm_ioremap(priv->dev, tfregs, 0x100);
	if (priv->tfregs == NULL) {
		dev_err(priv->dev, "Failed to map ATA ports\n");
		return -ENOMEM;
	}
	priv->host->iomap = &priv->tfregs;

	
	if (dmaregs != 0) {
		dma_regs = devm_ioremap(priv->dev, dmaregs,
					sizeof(struct dbdma_regs));
		if (dma_regs == NULL)
			dev_warn(priv->dev, "Failed to map ATA DMA registers\n");
	}

	
	if (fcregs != 0) {
		priv->kauai_fcr = devm_ioremap(priv->dev, fcregs, 4);
		if (priv->kauai_fcr == NULL) {
			dev_err(priv->dev, "Failed to map ATA FCR register\n");
			return -ENOMEM;
		}
	}

	
	pata_macio_setup_ios(&priv->host->ports[0]->ioaddr,
			     priv->tfregs, dma_regs);
	priv->host->ports[0]->private_data = priv;

	
	pata_macio_reset_hw(priv, 0);
	pata_macio_apply_timings(priv->host->ports[0], 0);

	
	if (priv->pdev && dma_regs)
		pci_set_master(priv->pdev);

	dev_info(priv->dev, "Activating pata-macio chipset %s, Apple bus ID %d\n",
		 macio_ata_names[priv->kind], priv->aapl_bus_id);

	
	priv->irq = irq;
	return ata_host_activate(priv->host, irq, ata_bmdma_interrupt, 0,
				 &pata_macio_sht);
}

static int __devinit pata_macio_attach(struct macio_dev *mdev,
				       const struct of_device_id *match)
{
	struct pata_macio_priv	*priv;
	resource_size_t		tfregs, dmaregs = 0;
	unsigned long		irq;
	int			rc;

	
	if (macio_resource_count(mdev) == 0) {
		dev_err(&mdev->ofdev.dev,
			"No addresses for controller\n");
		return -ENXIO;
	}

	
	macio_enable_devres(mdev);

	
	priv = devm_kzalloc(&mdev->ofdev.dev,
			    sizeof(struct pata_macio_priv), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&mdev->ofdev.dev,
			"Failed to allocate private memory\n");
		return -ENOMEM;
	}
	priv->node = of_node_get(mdev->ofdev.dev.of_node);
	priv->mdev = mdev;
	priv->dev = &mdev->ofdev.dev;

	
	if (macio_request_resource(mdev, 0, "pata-macio")) {
		dev_err(&mdev->ofdev.dev,
			"Cannot obtain taskfile resource\n");
		return -EBUSY;
	}
	tfregs = macio_resource_start(mdev, 0);

	
	if (macio_resource_count(mdev) >= 2) {
		if (macio_request_resource(mdev, 1, "pata-macio-dma"))
			dev_err(&mdev->ofdev.dev,
				"Cannot obtain DMA resource\n");
		else
			dmaregs = macio_resource_start(mdev, 1);
	}

	if (macio_irq_count(mdev) == 0) {
		dev_warn(&mdev->ofdev.dev,
			 "No interrupts for controller, using 13\n");
		irq = irq_create_mapping(NULL, 13);
	} else
		irq = macio_irq(mdev, 0);

	
	lock_media_bay(priv->mdev->media_bay);

	
	rc = pata_macio_common_init(priv,
				    tfregs,		
				    dmaregs,		
				    0,			
				    irq);
	unlock_media_bay(priv->mdev->media_bay);

	return rc;
}

static int __devexit pata_macio_detach(struct macio_dev *mdev)
{
	struct ata_host *host = macio_get_drvdata(mdev);
	struct pata_macio_priv *priv = host->private_data;

	lock_media_bay(priv->mdev->media_bay);

	priv->host->private_data = NULL;

	ata_host_detach(host);

	unlock_media_bay(priv->mdev->media_bay);

	return 0;
}

#ifdef CONFIG_PM

static int pata_macio_suspend(struct macio_dev *mdev, pm_message_t mesg)
{
	struct ata_host *host = macio_get_drvdata(mdev);

	return pata_macio_do_suspend(host->private_data, mesg);
}

static int pata_macio_resume(struct macio_dev *mdev)
{
	struct ata_host *host = macio_get_drvdata(mdev);

	return pata_macio_do_resume(host->private_data);
}

#endif 

#ifdef CONFIG_PMAC_MEDIABAY
static void pata_macio_mb_event(struct macio_dev* mdev, int mb_state)
{
	struct ata_host *host = macio_get_drvdata(mdev);
	struct ata_port *ap;
	struct ata_eh_info *ehi;
	struct ata_device *dev;
	unsigned long flags;

	if (!host || !host->private_data)
		return;
	ap = host->ports[0];
	spin_lock_irqsave(ap->lock, flags);
	ehi = &ap->link.eh_info;
	if (mb_state == MB_CD) {
		ata_ehi_push_desc(ehi, "mediabay plug");
		ata_ehi_hotplugged(ehi);
		ata_port_freeze(ap);
	} else {
		ata_ehi_push_desc(ehi, "mediabay unplug");
		ata_for_each_dev(dev, &ap->link, ALL)
			dev->flags |= ATA_DFLAG_DETACH;
		ata_port_abort(ap);
	}
	spin_unlock_irqrestore(ap->lock, flags);

}
#endif 


static int __devinit pata_macio_pci_attach(struct pci_dev *pdev,
					   const struct pci_device_id *id)
{
	struct pata_macio_priv	*priv;
	struct device_node	*np;
	resource_size_t		rbase;

	
	np = pci_device_to_OF_node(pdev);
	if (np == NULL) {
		dev_err(&pdev->dev,
			"Cannot find OF device node for controller\n");
		return -ENODEV;
	}

	
	if (pcim_enable_device(pdev)) {
		dev_err(&pdev->dev,
			"Cannot enable controller PCI device\n");
		return -ENXIO;
	}

	
	priv = devm_kzalloc(&pdev->dev,
			    sizeof(struct pata_macio_priv), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&pdev->dev,
			"Failed to allocate private memory\n");
		return -ENOMEM;
	}
	priv->node = of_node_get(np);
	priv->pdev = pdev;
	priv->dev = &pdev->dev;

	
	if (pci_request_regions(pdev, "pata-macio")) {
		dev_err(&pdev->dev,
			"Cannot obtain PCI resources\n");
		return -EBUSY;
	}

	
	rbase = pci_resource_start(pdev, 0);
	if (pata_macio_common_init(priv,
				   rbase + 0x2000,	
				   rbase + 0x1000,	
				   rbase,		
				   pdev->irq))
		return -ENXIO;

	return 0;
}

static void __devexit pata_macio_pci_detach(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);

	ata_host_detach(host);
}

#ifdef CONFIG_PM

static int pata_macio_pci_suspend(struct pci_dev *pdev, pm_message_t mesg)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);

	return pata_macio_do_suspend(host->private_data, mesg);
}

static int pata_macio_pci_resume(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);

	return pata_macio_do_resume(host->private_data);
}

#endif 

static struct of_device_id pata_macio_match[] =
{
	{
	.name 		= "IDE",
	},
	{
	.name 		= "ATA",
	},
	{
	.type		= "ide",
	},
	{
	.type		= "ata",
	},
	{},
};

static struct macio_driver pata_macio_driver =
{
	.driver = {
		.name 		= "pata-macio",
		.owner		= THIS_MODULE,
		.of_match_table	= pata_macio_match,
	},
	.probe		= pata_macio_attach,
	.remove		= pata_macio_detach,
#ifdef CONFIG_PM
	.suspend	= pata_macio_suspend,
	.resume		= pata_macio_resume,
#endif
#ifdef CONFIG_PMAC_MEDIABAY
	.mediabay_event	= pata_macio_mb_event,
#endif
};

static const struct pci_device_id pata_macio_pci_match[] = {
	{ PCI_VDEVICE(APPLE, PCI_DEVICE_ID_APPLE_UNI_N_ATA),	0 },
	{ PCI_VDEVICE(APPLE, PCI_DEVICE_ID_APPLE_IPID_ATA100),	0 },
	{ PCI_VDEVICE(APPLE, PCI_DEVICE_ID_APPLE_K2_ATA100),	0 },
	{ PCI_VDEVICE(APPLE, PCI_DEVICE_ID_APPLE_SH_ATA),	0 },
	{ PCI_VDEVICE(APPLE, PCI_DEVICE_ID_APPLE_IPID2_ATA),	0 },
	{},
};

static struct pci_driver pata_macio_pci_driver = {
	.name		= "pata-pci-macio",
	.id_table	= pata_macio_pci_match,
	.probe		= pata_macio_pci_attach,
	.remove		= pata_macio_pci_detach,
#ifdef CONFIG_PM
	.suspend	= pata_macio_pci_suspend,
	.resume		= pata_macio_pci_resume,
#endif
	.driver = {
		.owner		= THIS_MODULE,
	},
};
MODULE_DEVICE_TABLE(pci, pata_macio_pci_match);


static int __init pata_macio_init(void)
{
	int rc;

	if (!machine_is(powermac))
		return -ENODEV;

	rc = pci_register_driver(&pata_macio_pci_driver);
	if (rc)
		return rc;
	rc = macio_register_driver(&pata_macio_driver);
	if (rc) {
		pci_unregister_driver(&pata_macio_pci_driver);
		return rc;
	}
	return 0;
}

static void __exit pata_macio_exit(void)
{
	macio_unregister_driver(&pata_macio_driver);
	pci_unregister_driver(&pata_macio_pci_driver);
}

module_init(pata_macio_init);
module_exit(pata_macio_exit);

MODULE_AUTHOR("Benjamin Herrenschmidt");
MODULE_DESCRIPTION("Apple MacIO PATA driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
