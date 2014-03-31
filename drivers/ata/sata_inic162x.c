/*
 * sata_inic162x.c - Driver for Initio 162x SATA controllers
 *
 * Copyright 2006  SUSE Linux Products GmbH
 * Copyright 2006  Tejun Heo <teheo@novell.com>
 *
 * This file is released under GPL v2.
 *
 * This controller is eccentric and easily locks up if something isn't
 * right.  Documentation is available at initio's website but it only
 * documents registers (not programming model).
 *
 * This driver has interesting history.  The first version was written
 * from the documentation and a 2.4 IDE driver posted on a Taiwan
 * company, which didn't use any IDMA features and couldn't handle
 * LBA48.  The resulting driver couldn't handle LBA48 devices either
 * making it pretty useless.
 *
 * After a while, initio picked the driver up, renamed it to
 * sata_initio162x, updated it to use IDMA for ATA DMA commands and
 * posted it on their website.  It only used ATA_PROT_DMA for IDMA and
 * attaching both devices and issuing IDMA and !IDMA commands
 * simultaneously broke it due to PIRQ masking interaction but it did
 * show how to use the IDMA (ADMA + some initio specific twists)
 * engine.
 *
 * Then, I picked up their changes again and here's the usable driver
 * which uses IDMA for everything.  Everything works now including
 * LBA48, CD/DVD burning, suspend/resume and hotplug.  There are some
 * issues tho.  Result Tf is not resported properly, NCQ isn't
 * supported yet and CD/DVD writing works with DMA assisted PIO
 * protocol (which, for native SATA devices, shouldn't cause any
 * noticeable difference).
 *
 * Anyways, so, here's finally a working driver for inic162x.  Enjoy!
 *
 * initio: If you guys wanna improve the driver regarding result TF
 * access and other stuff, please feel free to contact me.  I'll be
 * happy to assist.
 */

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <linux/blkdev.h>
#include <scsi/scsi_device.h>

#define DRV_NAME	"sata_inic162x"
#define DRV_VERSION	"0.4"

enum {
	MMIO_BAR_PCI		= 5,
	MMIO_BAR_CARDBUS	= 1,

	NR_PORTS		= 2,

	IDMA_CPB_TBL_SIZE	= 4 * 32,

	INIC_DMA_BOUNDARY	= 0xffffff,

	HOST_ACTRL		= 0x08,
	HOST_CTL		= 0x7c,
	HOST_STAT		= 0x7e,
	HOST_IRQ_STAT		= 0xbc,
	HOST_IRQ_MASK		= 0xbe,

	PORT_SIZE		= 0x40,

	
	PORT_TF_DATA		= 0x00,
	PORT_TF_FEATURE		= 0x01,
	PORT_TF_NSECT		= 0x02,
	PORT_TF_LBAL		= 0x03,
	PORT_TF_LBAM		= 0x04,
	PORT_TF_LBAH		= 0x05,
	PORT_TF_DEVICE		= 0x06,
	PORT_TF_COMMAND		= 0x07,
	PORT_TF_ALT_STAT	= 0x08,
	PORT_IRQ_STAT		= 0x09,
	PORT_IRQ_MASK		= 0x0a,
	PORT_PRD_CTL		= 0x0b,
	PORT_PRD_ADDR		= 0x0c,
	PORT_PRD_XFERLEN	= 0x10,
	PORT_CPB_CPBLAR		= 0x18,
	PORT_CPB_PTQFIFO	= 0x1c,

	
	PORT_IDMA_CTL		= 0x14,
	PORT_IDMA_STAT		= 0x16,

	PORT_RPQ_FIFO		= 0x1e,
	PORT_RPQ_CNT		= 0x1f,

	PORT_SCR		= 0x20,

	
	HCTL_LEDEN		= (1 << 3),  
	HCTL_IRQOFF		= (1 << 8),  
	HCTL_FTHD0		= (1 << 10), 
	HCTL_FTHD1		= (1 << 11), 
	HCTL_PWRDWN		= (1 << 12), 
	HCTL_SOFTRST		= (1 << 13), 
	HCTL_RPGSEL		= (1 << 15), 

	HCTL_KNOWN_BITS		= HCTL_IRQOFF | HCTL_PWRDWN | HCTL_SOFTRST |
				  HCTL_RPGSEL,

	
	HIRQ_PORT0		= (1 << 0),
	HIRQ_PORT1		= (1 << 1),
	HIRQ_SOFT		= (1 << 14),
	HIRQ_GLOBAL		= (1 << 15), 

	
	PIRQ_OFFLINE		= (1 << 0),  
	PIRQ_ONLINE		= (1 << 1),  
	PIRQ_COMPLETE		= (1 << 2),  
	PIRQ_FATAL		= (1 << 3),  
	PIRQ_ATA		= (1 << 4),  
	PIRQ_REPLY		= (1 << 5),  
	PIRQ_PENDING		= (1 << 7),  

	PIRQ_ERR		= PIRQ_OFFLINE | PIRQ_ONLINE | PIRQ_FATAL,
	PIRQ_MASK_DEFAULT	= PIRQ_REPLY | PIRQ_ATA,
	PIRQ_MASK_FREEZE	= 0xff,

	
	PRD_CTL_START		= (1 << 0),
	PRD_CTL_WR		= (1 << 3),
	PRD_CTL_DMAEN		= (1 << 7),  

	
	IDMA_CTL_RST_ATA	= (1 << 2),  
	IDMA_CTL_RST_IDMA	= (1 << 5),  
	IDMA_CTL_GO		= (1 << 7),  
	IDMA_CTL_ATA_NIEN	= (1 << 8),  

	
	IDMA_STAT_PERR		= (1 << 0),  
	IDMA_STAT_CPBERR	= (1 << 1),  
	IDMA_STAT_LGCY		= (1 << 3),  
	IDMA_STAT_UIRQ		= (1 << 4),  
	IDMA_STAT_STPD		= (1 << 5),  
	IDMA_STAT_PSD		= (1 << 6),  
	IDMA_STAT_DONE		= (1 << 7),  

	IDMA_STAT_ERR		= IDMA_STAT_PERR | IDMA_STAT_CPBERR,

	
	CPB_CTL_VALID		= (1 << 0),  
	CPB_CTL_QUEUED		= (1 << 1),  
	CPB_CTL_DATA		= (1 << 2),  
	CPB_CTL_IEN		= (1 << 3),  
	CPB_CTL_DEVDIR		= (1 << 4),  

	
	CPB_RESP_DONE		= (1 << 0),  
	CPB_RESP_REL		= (1 << 1),  
	CPB_RESP_IGNORED	= (1 << 2),  
	CPB_RESP_ATA_ERR	= (1 << 3),  
	CPB_RESP_SPURIOUS	= (1 << 4),  
	CPB_RESP_UNDERFLOW	= (1 << 5),  
	CPB_RESP_OVERFLOW	= (1 << 6),  
	CPB_RESP_CPB_ERR	= (1 << 7),  

	
	PRD_DRAIN		= (1 << 1),  
	PRD_CDB			= (1 << 2),  
	PRD_DIRECT_INTR		= (1 << 3),  
	PRD_DMA			= (1 << 4),  
	PRD_WRITE		= (1 << 5),  
	PRD_IOM			= (1 << 6),  
	PRD_END			= (1 << 7),  
};

struct inic_cpb {
	u8		resp_flags;	
	u8		error;		
	u8		status;		
	u8		ctl_flags;	
	__le32		len;		
	__le32		prd;		
	u8		rsvd[4];
	
	u8		feature;	
	u8		hob_feature;	
	u8		device;		
	u8		mirctl;		
	u8		nsect;		
	u8		hob_nsect;	
	u8		lbal;		
	u8		hob_lbal;	
	u8		lbam;		
	u8		hob_lbam;	
	u8		lbah;		
	u8		hob_lbah;	
	u8		command;	
	u8		ctl;		
	u8		slave_error;	
	u8		slave_status;	
	
} __packed;

struct inic_prd {
	__le32		mad;		
	__le16		len;		
	u8		rsvd;
	u8		flags;		
} __packed;

struct inic_pkt {
	struct inic_cpb	cpb;
	struct inic_prd	prd[LIBATA_MAX_PRD + 1];	
	u8		cdb[ATAPI_CDB_LEN];
} __packed;

struct inic_host_priv {
	void __iomem	*mmio_base;
	u16		cached_hctl;
};

struct inic_port_priv {
	struct inic_pkt	*pkt;
	dma_addr_t	pkt_dma;
	u32		*cpb_tbl;
	dma_addr_t	cpb_tbl_dma;
};

static struct scsi_host_template inic_sht = {
	ATA_BASE_SHT(DRV_NAME),
	.sg_tablesize	= LIBATA_MAX_PRD,	
	.dma_boundary	= INIC_DMA_BOUNDARY,
};

static const int scr_map[] = {
	[SCR_STATUS]	= 0,
	[SCR_ERROR]	= 1,
	[SCR_CONTROL]	= 2,
};

static void __iomem *inic_port_base(struct ata_port *ap)
{
	struct inic_host_priv *hpriv = ap->host->private_data;

	return hpriv->mmio_base + ap->port_no * PORT_SIZE;
}

static void inic_reset_port(void __iomem *port_base)
{
	void __iomem *idma_ctl = port_base + PORT_IDMA_CTL;

	
	readw(idma_ctl); 
	msleep(1);

	
	writew(IDMA_CTL_RST_IDMA, idma_ctl);
	readw(idma_ctl); 
	msleep(1);

	
	writew(0, idma_ctl);

	
	writeb(0xff, port_base + PORT_IRQ_STAT);
}

static int inic_scr_read(struct ata_link *link, unsigned sc_reg, u32 *val)
{
	void __iomem *scr_addr = inic_port_base(link->ap) + PORT_SCR;
	void __iomem *addr;

	if (unlikely(sc_reg >= ARRAY_SIZE(scr_map)))
		return -EINVAL;

	addr = scr_addr + scr_map[sc_reg] * 4;
	*val = readl(scr_addr + scr_map[sc_reg] * 4);

	
	if (sc_reg == SCR_ERROR)
		*val &= ~SERR_PHYRDY_CHG;
	return 0;
}

static int inic_scr_write(struct ata_link *link, unsigned sc_reg, u32 val)
{
	void __iomem *scr_addr = inic_port_base(link->ap) + PORT_SCR;

	if (unlikely(sc_reg >= ARRAY_SIZE(scr_map)))
		return -EINVAL;

	writel(val, scr_addr + scr_map[sc_reg] * 4);
	return 0;
}

static void inic_stop_idma(struct ata_port *ap)
{
	void __iomem *port_base = inic_port_base(ap);

	readb(port_base + PORT_RPQ_FIFO);
	readb(port_base + PORT_RPQ_CNT);
	writew(0, port_base + PORT_IDMA_CTL);
}

static void inic_host_err_intr(struct ata_port *ap, u8 irq_stat, u16 idma_stat)
{
	struct ata_eh_info *ehi = &ap->link.eh_info;
	struct inic_port_priv *pp = ap->private_data;
	struct inic_cpb *cpb = &pp->pkt->cpb;
	bool freeze = false;

	ata_ehi_clear_desc(ehi);
	ata_ehi_push_desc(ehi, "irq_stat=0x%x idma_stat=0x%x",
			  irq_stat, idma_stat);

	inic_stop_idma(ap);

	if (irq_stat & (PIRQ_OFFLINE | PIRQ_ONLINE)) {
		ata_ehi_push_desc(ehi, "hotplug");
		ata_ehi_hotplugged(ehi);
		freeze = true;
	}

	if (idma_stat & IDMA_STAT_PERR) {
		ata_ehi_push_desc(ehi, "PCI error");
		freeze = true;
	}

	if (idma_stat & IDMA_STAT_CPBERR) {
		ata_ehi_push_desc(ehi, "CPB error");

		if (cpb->resp_flags & CPB_RESP_IGNORED) {
			__ata_ehi_push_desc(ehi, " ignored");
			ehi->err_mask |= AC_ERR_INVALID;
			freeze = true;
		}

		if (cpb->resp_flags & CPB_RESP_ATA_ERR)
			ehi->err_mask |= AC_ERR_DEV;

		if (cpb->resp_flags & CPB_RESP_SPURIOUS) {
			__ata_ehi_push_desc(ehi, " spurious-intr");
			ehi->err_mask |= AC_ERR_HSM;
			freeze = true;
		}

		if (cpb->resp_flags &
		    (CPB_RESP_UNDERFLOW | CPB_RESP_OVERFLOW)) {
			__ata_ehi_push_desc(ehi, " data-over/underflow");
			ehi->err_mask |= AC_ERR_HSM;
			freeze = true;
		}
	}

	if (freeze)
		ata_port_freeze(ap);
	else
		ata_port_abort(ap);
}

static void inic_host_intr(struct ata_port *ap)
{
	void __iomem *port_base = inic_port_base(ap);
	struct ata_queued_cmd *qc = ata_qc_from_tag(ap, ap->link.active_tag);
	u8 irq_stat;
	u16 idma_stat;

	
	irq_stat = readb(port_base + PORT_IRQ_STAT);
	writeb(irq_stat, port_base + PORT_IRQ_STAT);
	idma_stat = readw(port_base + PORT_IDMA_STAT);

	if (unlikely((irq_stat & PIRQ_ERR) || (idma_stat & IDMA_STAT_ERR)))
		inic_host_err_intr(ap, irq_stat, idma_stat);

	if (unlikely(!qc))
		goto spurious;

	if (likely(idma_stat & IDMA_STAT_DONE)) {
		inic_stop_idma(ap);

		if (unlikely(readb(port_base + PORT_TF_COMMAND) &
			     (ATA_DF | ATA_ERR)))
			qc->err_mask |= AC_ERR_DEV;

		ata_qc_complete(qc);
		return;
	}

 spurious:
	ata_port_warn(ap, "unhandled interrupt: cmd=0x%x irq_stat=0x%x idma_stat=0x%x\n",
		      qc ? qc->tf.command : 0xff, irq_stat, idma_stat);
}

static irqreturn_t inic_interrupt(int irq, void *dev_instance)
{
	struct ata_host *host = dev_instance;
	struct inic_host_priv *hpriv = host->private_data;
	u16 host_irq_stat;
	int i, handled = 0;

	host_irq_stat = readw(hpriv->mmio_base + HOST_IRQ_STAT);

	if (unlikely(!(host_irq_stat & HIRQ_GLOBAL)))
		goto out;

	spin_lock(&host->lock);

	for (i = 0; i < NR_PORTS; i++)
		if (host_irq_stat & (HIRQ_PORT0 << i)) {
			inic_host_intr(host->ports[i]);
			handled++;
		}

	spin_unlock(&host->lock);

 out:
	return IRQ_RETVAL(handled);
}

static int inic_check_atapi_dma(struct ata_queued_cmd *qc)
{
	if (atapi_cmd_type(qc->cdb[0]) == READ)
		return 0;
	return 1;
}

static void inic_fill_sg(struct inic_prd *prd, struct ata_queued_cmd *qc)
{
	struct scatterlist *sg;
	unsigned int si;
	u8 flags = 0;

	if (qc->tf.flags & ATA_TFLAG_WRITE)
		flags |= PRD_WRITE;

	if (ata_is_dma(qc->tf.protocol))
		flags |= PRD_DMA;

	for_each_sg(qc->sg, sg, qc->n_elem, si) {
		prd->mad = cpu_to_le32(sg_dma_address(sg));
		prd->len = cpu_to_le16(sg_dma_len(sg));
		prd->flags = flags;
		prd++;
	}

	WARN_ON(!si);
	prd[-1].flags |= PRD_END;
}

static void inic_qc_prep(struct ata_queued_cmd *qc)
{
	struct inic_port_priv *pp = qc->ap->private_data;
	struct inic_pkt *pkt = pp->pkt;
	struct inic_cpb *cpb = &pkt->cpb;
	struct inic_prd *prd = pkt->prd;
	bool is_atapi = ata_is_atapi(qc->tf.protocol);
	bool is_data = ata_is_data(qc->tf.protocol);
	unsigned int cdb_len = 0;

	VPRINTK("ENTER\n");

	if (is_atapi)
		cdb_len = qc->dev->cdb_len;

	
	memset(pkt, 0, sizeof(struct inic_pkt));

	cpb->ctl_flags = CPB_CTL_VALID | CPB_CTL_IEN;
	if (is_atapi || is_data)
		cpb->ctl_flags |= CPB_CTL_DATA;

	cpb->len = cpu_to_le32(qc->nbytes + cdb_len);
	cpb->prd = cpu_to_le32(pp->pkt_dma + offsetof(struct inic_pkt, prd));

	cpb->device = qc->tf.device;
	cpb->feature = qc->tf.feature;
	cpb->nsect = qc->tf.nsect;
	cpb->lbal = qc->tf.lbal;
	cpb->lbam = qc->tf.lbam;
	cpb->lbah = qc->tf.lbah;

	if (qc->tf.flags & ATA_TFLAG_LBA48) {
		cpb->hob_feature = qc->tf.hob_feature;
		cpb->hob_nsect = qc->tf.hob_nsect;
		cpb->hob_lbal = qc->tf.hob_lbal;
		cpb->hob_lbam = qc->tf.hob_lbam;
		cpb->hob_lbah = qc->tf.hob_lbah;
	}

	cpb->command = qc->tf.command;
	

	
	if (is_atapi) {
		memcpy(pkt->cdb, qc->cdb, ATAPI_CDB_LEN);
		prd->mad = cpu_to_le32(pp->pkt_dma +
				       offsetof(struct inic_pkt, cdb));
		prd->len = cpu_to_le16(cdb_len);
		prd->flags = PRD_CDB | PRD_WRITE;
		if (!is_data)
			prd->flags |= PRD_END;
		prd++;
	}

	
	if (is_data)
		inic_fill_sg(prd, qc);

	pp->cpb_tbl[0] = pp->pkt_dma;
}

static unsigned int inic_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	void __iomem *port_base = inic_port_base(ap);

	
	writew(HCTL_FTHD0 | HCTL_LEDEN, port_base + HOST_CTL);
	writew(IDMA_CTL_GO, port_base + PORT_IDMA_CTL);
	writeb(0, port_base + PORT_CPB_PTQFIFO);

	return 0;
}

static void inic_tf_read(struct ata_port *ap, struct ata_taskfile *tf)
{
	void __iomem *port_base = inic_port_base(ap);

	tf->feature	= readb(port_base + PORT_TF_FEATURE);
	tf->nsect	= readb(port_base + PORT_TF_NSECT);
	tf->lbal	= readb(port_base + PORT_TF_LBAL);
	tf->lbam	= readb(port_base + PORT_TF_LBAM);
	tf->lbah	= readb(port_base + PORT_TF_LBAH);
	tf->device	= readb(port_base + PORT_TF_DEVICE);
	tf->command	= readb(port_base + PORT_TF_COMMAND);
}

static bool inic_qc_fill_rtf(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *rtf = &qc->result_tf;
	struct ata_taskfile tf;

	inic_tf_read(qc->ap, &tf);

	if (!(tf.command & ATA_ERR))
		return false;

	rtf->command = tf.command;
	rtf->feature = tf.feature;
	return true;
}

static void inic_freeze(struct ata_port *ap)
{
	void __iomem *port_base = inic_port_base(ap);

	writeb(PIRQ_MASK_FREEZE, port_base + PORT_IRQ_MASK);
	writeb(0xff, port_base + PORT_IRQ_STAT);
}

static void inic_thaw(struct ata_port *ap)
{
	void __iomem *port_base = inic_port_base(ap);

	writeb(0xff, port_base + PORT_IRQ_STAT);
	writeb(PIRQ_MASK_DEFAULT, port_base + PORT_IRQ_MASK);
}

static int inic_check_ready(struct ata_link *link)
{
	void __iomem *port_base = inic_port_base(link->ap);

	return ata_check_ready(readb(port_base + PORT_TF_COMMAND));
}

static int inic_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	void __iomem *port_base = inic_port_base(ap);
	void __iomem *idma_ctl = port_base + PORT_IDMA_CTL;
	const unsigned long *timing = sata_ehc_deb_timing(&link->eh_context);
	int rc;

	
	inic_reset_port(port_base);

	writew(IDMA_CTL_RST_ATA, idma_ctl);
	readw(idma_ctl);	
	ata_msleep(ap, 1);
	writew(0, idma_ctl);

	rc = sata_link_resume(link, timing, deadline);
	if (rc) {
		ata_link_warn(link,
			      "failed to resume link after reset (errno=%d)\n",
			      rc);
		return rc;
	}

	*class = ATA_DEV_NONE;
	if (ata_link_online(link)) {
		struct ata_taskfile tf;

		
		rc = ata_wait_after_reset(link, deadline, inic_check_ready);
		
		if (rc) {
			ata_link_warn(link,
				      "device not ready after hardreset (errno=%d)\n",
				      rc);
			return rc;
		}

		inic_tf_read(ap, &tf);
		*class = ata_dev_classify(&tf);
	}

	return 0;
}

static void inic_error_handler(struct ata_port *ap)
{
	void __iomem *port_base = inic_port_base(ap);

	inic_reset_port(port_base);
	ata_std_error_handler(ap);
}

static void inic_post_internal_cmd(struct ata_queued_cmd *qc)
{
	
	if (qc->flags & ATA_QCFLAG_FAILED)
		inic_reset_port(inic_port_base(qc->ap));
}

static void init_port(struct ata_port *ap)
{
	void __iomem *port_base = inic_port_base(ap);
	struct inic_port_priv *pp = ap->private_data;

	
	memset(pp->pkt, 0, sizeof(struct inic_pkt));
	memset(pp->cpb_tbl, 0, IDMA_CPB_TBL_SIZE);

	
	writel(pp->cpb_tbl_dma, port_base + PORT_CPB_CPBLAR);
}

static int inic_port_resume(struct ata_port *ap)
{
	init_port(ap);
	return 0;
}

static int inic_port_start(struct ata_port *ap)
{
	struct device *dev = ap->host->dev;
	struct inic_port_priv *pp;

	
	pp = devm_kzalloc(dev, sizeof(*pp), GFP_KERNEL);
	if (!pp)
		return -ENOMEM;
	ap->private_data = pp;

	
	pp->pkt = dmam_alloc_coherent(dev, sizeof(struct inic_pkt),
				      &pp->pkt_dma, GFP_KERNEL);
	if (!pp->pkt)
		return -ENOMEM;

	pp->cpb_tbl = dmam_alloc_coherent(dev, IDMA_CPB_TBL_SIZE,
					  &pp->cpb_tbl_dma, GFP_KERNEL);
	if (!pp->cpb_tbl)
		return -ENOMEM;

	init_port(ap);

	return 0;
}

static struct ata_port_operations inic_port_ops = {
	.inherits		= &sata_port_ops,

	.check_atapi_dma	= inic_check_atapi_dma,
	.qc_prep		= inic_qc_prep,
	.qc_issue		= inic_qc_issue,
	.qc_fill_rtf		= inic_qc_fill_rtf,

	.freeze			= inic_freeze,
	.thaw			= inic_thaw,
	.hardreset		= inic_hardreset,
	.error_handler		= inic_error_handler,
	.post_internal_cmd	= inic_post_internal_cmd,

	.scr_read		= inic_scr_read,
	.scr_write		= inic_scr_write,

	.port_resume		= inic_port_resume,
	.port_start		= inic_port_start,
};

static struct ata_port_info inic_port_info = {
	.flags			= ATA_FLAG_SATA | ATA_FLAG_PIO_DMA,
	.pio_mask		= ATA_PIO4,
	.mwdma_mask		= ATA_MWDMA2,
	.udma_mask		= ATA_UDMA6,
	.port_ops		= &inic_port_ops
};

static int init_controller(void __iomem *mmio_base, u16 hctl)
{
	int i;
	u16 val;

	hctl &= ~HCTL_KNOWN_BITS;

	writew(hctl | HCTL_SOFTRST, mmio_base + HOST_CTL);
	readw(mmio_base + HOST_CTL); 

	for (i = 0; i < 10; i++) {
		msleep(1);
		val = readw(mmio_base + HOST_CTL);
		if (!(val & HCTL_SOFTRST))
			break;
	}

	if (val & HCTL_SOFTRST)
		return -EIO;

	
	for (i = 0; i < NR_PORTS; i++) {
		void __iomem *port_base = mmio_base + i * PORT_SIZE;

		writeb(0xff, port_base + PORT_IRQ_MASK);
		inic_reset_port(port_base);
	}

	
	writew(hctl & ~HCTL_IRQOFF, mmio_base + HOST_CTL);
	val = readw(mmio_base + HOST_IRQ_MASK);
	val &= ~(HIRQ_PORT0 | HIRQ_PORT1);
	writew(val, mmio_base + HOST_IRQ_MASK);

	return 0;
}

#ifdef CONFIG_PM
static int inic_pci_device_resume(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	struct inic_host_priv *hpriv = host->private_data;
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc)
		return rc;

	if (pdev->dev.power.power_state.event == PM_EVENT_SUSPEND) {
		rc = init_controller(hpriv->mmio_base, hpriv->cached_hctl);
		if (rc)
			return rc;
	}

	ata_host_resume(host);

	return 0;
}
#endif

static int inic_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	const struct ata_port_info *ppi[] = { &inic_port_info, NULL };
	struct ata_host *host;
	struct inic_host_priv *hpriv;
	void __iomem * const *iomap;
	int mmio_bar;
	int i, rc;

	ata_print_version_once(&pdev->dev, DRV_VERSION);

	
	host = ata_host_alloc_pinfo(&pdev->dev, ppi, NR_PORTS);
	hpriv = devm_kzalloc(&pdev->dev, sizeof(*hpriv), GFP_KERNEL);
	if (!host || !hpriv)
		return -ENOMEM;

	host->private_data = hpriv;

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	if (pci_resource_flags(pdev, MMIO_BAR_PCI) & IORESOURCE_MEM)
		mmio_bar = MMIO_BAR_PCI;
	else
		mmio_bar = MMIO_BAR_CARDBUS;

	rc = pcim_iomap_regions(pdev, 1 << mmio_bar, DRV_NAME);
	if (rc)
		return rc;
	host->iomap = iomap = pcim_iomap_table(pdev);
	hpriv->mmio_base = iomap[mmio_bar];
	hpriv->cached_hctl = readw(hpriv->mmio_base + HOST_CTL);

	for (i = 0; i < NR_PORTS; i++) {
		struct ata_port *ap = host->ports[i];

		ata_port_pbar_desc(ap, mmio_bar, -1, "mmio");
		ata_port_pbar_desc(ap, mmio_bar, i * PORT_SIZE, "port");
	}

	
	rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (rc) {
		dev_err(&pdev->dev, "32-bit DMA enable failed\n");
		return rc;
	}

	rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
	if (rc) {
		dev_err(&pdev->dev, "32-bit consistent DMA enable failed\n");
		return rc;
	}

	rc = pci_set_dma_max_seg_size(pdev, 65536 - 512);
	if (rc) {
		dev_err(&pdev->dev, "failed to set the maximum segment size\n");
		return rc;
	}

	rc = init_controller(hpriv->mmio_base, hpriv->cached_hctl);
	if (rc) {
		dev_err(&pdev->dev, "failed to initialize controller\n");
		return rc;
	}

	pci_set_master(pdev);
	return ata_host_activate(host, pdev->irq, inic_interrupt, IRQF_SHARED,
				 &inic_sht);
}

static const struct pci_device_id inic_pci_tbl[] = {
	{ PCI_VDEVICE(INIT, 0x1622), },
	{ },
};

static struct pci_driver inic_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= inic_pci_tbl,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= inic_pci_device_resume,
#endif
	.probe 		= inic_init_one,
	.remove		= ata_pci_remove_one,
};

static int __init inic_init(void)
{
	return pci_register_driver(&inic_pci_driver);
}

static void __exit inic_exit(void)
{
	pci_unregister_driver(&inic_pci_driver);
}

MODULE_AUTHOR("Tejun Heo");
MODULE_DESCRIPTION("low-level driver for Initio 162x SATA");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(pci, inic_pci_tbl);
MODULE_VERSION(DRV_VERSION);

module_init(inic_init);
module_exit(inic_exit);
