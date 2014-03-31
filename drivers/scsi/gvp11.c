#include <linux/types.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/zorro.h>
#include <linux/module.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/amigaints.h>
#include <asm/amigahw.h>

#include "scsi.h"
#include "wd33c93.h"
#include "gvp11.h"


#define CHECK_WD33C93

struct gvp11_hostdata {
	struct WD33C93_hostdata wh;
	struct gvp11_scsiregs *regs;
};

static irqreturn_t gvp11_intr(int irq, void *data)
{
	struct Scsi_Host *instance = data;
	struct gvp11_hostdata *hdata = shost_priv(instance);
	unsigned int status = hdata->regs->CNTR;
	unsigned long flags;

	if (!(status & GVP11_DMAC_INT_PENDING))
		return IRQ_NONE;

	spin_lock_irqsave(instance->host_lock, flags);
	wd33c93_intr(instance);
	spin_unlock_irqrestore(instance->host_lock, flags);
	return IRQ_HANDLED;
}

static int gvp11_xfer_mask = 0;

void gvp11_setup(char *str, int *ints)
{
	gvp11_xfer_mask = ints[1];
}

static int dma_setup(struct scsi_cmnd *cmd, int dir_in)
{
	struct Scsi_Host *instance = cmd->device->host;
	struct gvp11_hostdata *hdata = shost_priv(instance);
	struct WD33C93_hostdata *wh = &hdata->wh;
	struct gvp11_scsiregs *regs = hdata->regs;
	unsigned short cntr = GVP11_DMAC_INT_ENABLE;
	unsigned long addr = virt_to_bus(cmd->SCp.ptr);
	int bank_mask;
	static int scsi_alloc_out_of_range = 0;

	
	if (addr & wh->dma_xfer_mask) {
		wh->dma_bounce_len = (cmd->SCp.this_residual + 511) & ~0x1ff;

		if (!scsi_alloc_out_of_range) {
			wh->dma_bounce_buffer =
				kmalloc(wh->dma_bounce_len, GFP_KERNEL);
			wh->dma_buffer_pool = BUF_SCSI_ALLOCED;
		}

		if (scsi_alloc_out_of_range ||
		    !wh->dma_bounce_buffer) {
			wh->dma_bounce_buffer =
				amiga_chip_alloc(wh->dma_bounce_len,
						 "GVP II SCSI Bounce Buffer");

			if (!wh->dma_bounce_buffer) {
				wh->dma_bounce_len = 0;
				return 1;
			}

			wh->dma_buffer_pool = BUF_CHIP_ALLOCED;
		}

		
		addr = virt_to_bus(wh->dma_bounce_buffer);

		if (addr & wh->dma_xfer_mask) {
			
			if (wh->dma_buffer_pool == BUF_SCSI_ALLOCED) {
				kfree(wh->dma_bounce_buffer);
				scsi_alloc_out_of_range = 1;
			} else {
				amiga_chip_free(wh->dma_bounce_buffer);
			}

			wh->dma_bounce_buffer =
				amiga_chip_alloc(wh->dma_bounce_len,
						 "GVP II SCSI Bounce Buffer");

			if (!wh->dma_bounce_buffer) {
				wh->dma_bounce_len = 0;
				return 1;
			}

			addr = virt_to_bus(wh->dma_bounce_buffer);
			wh->dma_buffer_pool = BUF_CHIP_ALLOCED;
		}

		if (!dir_in) {
			
			memcpy(wh->dma_bounce_buffer, cmd->SCp.ptr,
			       cmd->SCp.this_residual);
		}
	}

	
	if (!dir_in)
		cntr |= GVP11_DMAC_DIR_WRITE;

	wh->dma_dir = dir_in;
	regs->CNTR = cntr;

	
	regs->ACR = addr;

	if (dir_in) {
		
		cache_clear(addr, cmd->SCp.this_residual);
	} else {
		
		cache_push(addr, cmd->SCp.this_residual);
	}

	bank_mask = (~wh->dma_xfer_mask >> 18) & 0x01c0;
	if (bank_mask)
		regs->BANK = bank_mask & (addr >> 18);

	
	regs->ST_DMA = 1;

	
	return 0;
}

static void dma_stop(struct Scsi_Host *instance, struct scsi_cmnd *SCpnt,
		     int status)
{
	struct gvp11_hostdata *hdata = shost_priv(instance);
	struct WD33C93_hostdata *wh = &hdata->wh;
	struct gvp11_scsiregs *regs = hdata->regs;

	
	regs->SP_DMA = 1;
	
	regs->CNTR = GVP11_DMAC_INT_ENABLE;

	
	if (status && wh->dma_bounce_buffer) {
		if (wh->dma_dir && SCpnt)
			memcpy(SCpnt->SCp.ptr, wh->dma_bounce_buffer,
			       SCpnt->SCp.this_residual);

		if (wh->dma_buffer_pool == BUF_SCSI_ALLOCED)
			kfree(wh->dma_bounce_buffer);
		else
			amiga_chip_free(wh->dma_bounce_buffer);

		wh->dma_bounce_buffer = NULL;
		wh->dma_bounce_len = 0;
	}
}

static int gvp11_bus_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *instance = cmd->device->host;

	


	spin_lock_irq(instance->host_lock);
	wd33c93_host_reset(cmd);
	spin_unlock_irq(instance->host_lock);

	return SUCCESS;
}

static struct scsi_host_template gvp11_scsi_template = {
	.module			= THIS_MODULE,
	.name			= "GVP Series II SCSI",
	.proc_info		= wd33c93_proc_info,
	.proc_name		= "GVP11",
	.queuecommand		= wd33c93_queuecommand,
	.eh_abort_handler	= wd33c93_abort,
	.eh_bus_reset_handler	= gvp11_bus_reset,
	.eh_host_reset_handler	= wd33c93_host_reset,
	.can_queue		= CAN_QUEUE,
	.this_id		= 7,
	.sg_tablesize		= SG_ALL,
	.cmd_per_lun		= CMD_PER_LUN,
	.use_clustering		= DISABLE_CLUSTERING
};

static int __devinit check_wd33c93(struct gvp11_scsiregs *regs)
{
#ifdef CHECK_WD33C93
	volatile unsigned char *sasr_3393, *scmd_3393;
	unsigned char save_sasr;
	unsigned char q, qq;


	

	sasr_3393 = &regs->SASR;
	scmd_3393 = &regs->SCMD;
	save_sasr = *sasr_3393;

	

	q = *sasr_3393;	
	if (q & 0x08)	
		return -ENODEV;
	*sasr_3393 = WD_AUXILIARY_STATUS;	
	if (*sasr_3393 == WD_AUXILIARY_STATUS) {	
		*sasr_3393 = save_sasr;	
		return -ENODEV;
	}
	if (*sasr_3393 != q) {	
		*sasr_3393 = save_sasr;	
		return -ENODEV;
	}
	if (*scmd_3393 != q)	
		return -ENODEV;


	*sasr_3393 = WD_SCSI_STATUS;
	q = *scmd_3393;
	*sasr_3393 = WD_SCSI_STATUS;
	*scmd_3393 = ~q;
	*sasr_3393 = WD_SCSI_STATUS;
	qq = *scmd_3393;
	*sasr_3393 = WD_SCSI_STATUS;
	*scmd_3393 = q;
	if (qq != q)	
		return -ENODEV;
	*sasr_3393 = 0x1e;	
	q = *scmd_3393;
	*sasr_3393 = 0x1e;
	*scmd_3393 = ~q;
	*sasr_3393 = 0x1e;
	qq = *scmd_3393;
	*sasr_3393 = 0x1e;
	*scmd_3393 = q;
	if (qq != q || qq != 0xff)	
		return -ENODEV;
	*sasr_3393 = WD_TIMEOUT_PERIOD;
	q = *scmd_3393;
	*sasr_3393 = WD_TIMEOUT_PERIOD;
	*scmd_3393 = ~q;
	*sasr_3393 = WD_TIMEOUT_PERIOD;
	qq = *scmd_3393;
	*sasr_3393 = WD_TIMEOUT_PERIOD;
	*scmd_3393 = q;
	if (qq != (~q & 0xff))	
		return -ENODEV;
#endif 

	return 0;
}

static int __devinit gvp11_probe(struct zorro_dev *z,
				 const struct zorro_device_id *ent)
{
	struct Scsi_Host *instance;
	unsigned long address;
	int error;
	unsigned int epc;
	unsigned int default_dma_xfer_mask;
	struct gvp11_hostdata *hdata;
	struct gvp11_scsiregs *regs;
	wd33c93_regs wdregs;

	default_dma_xfer_mask = ent->driver_data;

	if (zorro_resource_len(z) != 0x10000)
		return -ENODEV;

	address = z->resource.start;
	if (!request_mem_region(address, 256, "wd33c93"))
		return -EBUSY;

	regs = (struct gvp11_scsiregs *)(ZTWO_VADDR(address));

	error = check_wd33c93(regs);
	if (error)
		goto fail_check_or_alloc;

	instance = scsi_host_alloc(&gvp11_scsi_template,
				   sizeof(struct gvp11_hostdata));
	if (!instance) {
		error = -ENOMEM;
		goto fail_check_or_alloc;
	}

	instance->irq = IRQ_AMIGA_PORTS;
	instance->unique_id = z->slotaddr;

	regs->secret2 = 1;
	regs->secret1 = 0;
	regs->secret3 = 15;
	while (regs->CNTR & GVP11_DMAC_BUSY)
		;
	regs->CNTR = 0;
	regs->BANK = 0;

	wdregs.SASR = &regs->SASR;
	wdregs.SCMD = &regs->SCMD;

	hdata = shost_priv(instance);
	if (gvp11_xfer_mask)
		hdata->wh.dma_xfer_mask = gvp11_xfer_mask;
	else
		hdata->wh.dma_xfer_mask = default_dma_xfer_mask;

	hdata->wh.no_sync = 0xff;
	hdata->wh.fast = 0;
	hdata->wh.dma_mode = CTRL_DMA;
	hdata->regs = regs;

	epc = *(unsigned short *)(ZTWO_VADDR(address) + 0x8000);
	wd33c93_init(instance, wdregs, dma_setup, dma_stop,
		     (epc & GVP_SCSICLKMASK) ? WD33C93_FS_8_10
					     : WD33C93_FS_12_15);

	error = request_irq(IRQ_AMIGA_PORTS, gvp11_intr, IRQF_SHARED,
			    "GVP11 SCSI", instance);
	if (error)
		goto fail_irq;

	regs->CNTR = GVP11_DMAC_INT_ENABLE;

	error = scsi_add_host(instance, NULL);
	if (error)
		goto fail_host;

	zorro_set_drvdata(z, instance);
	scsi_scan_host(instance);
	return 0;

fail_host:
	free_irq(IRQ_AMIGA_PORTS, instance);
fail_irq:
	scsi_host_put(instance);
fail_check_or_alloc:
	release_mem_region(address, 256);
	return error;
}

static void __devexit gvp11_remove(struct zorro_dev *z)
{
	struct Scsi_Host *instance = zorro_get_drvdata(z);
	struct gvp11_hostdata *hdata = shost_priv(instance);

	hdata->regs->CNTR = 0;
	scsi_remove_host(instance);
	free_irq(IRQ_AMIGA_PORTS, instance);
	scsi_host_put(instance);
	release_mem_region(z->resource.start, 256);
}


static struct zorro_device_id gvp11_zorro_tbl[] __devinitdata = {
	{ ZORRO_PROD_GVP_COMBO_030_R3_SCSI,	~0x00ffffff },
	{ ZORRO_PROD_GVP_SERIES_II,		~0x00ffffff },
	{ ZORRO_PROD_GVP_GFORCE_030_SCSI,	~0x01ffffff },
	{ ZORRO_PROD_GVP_A530_SCSI,		~0x01ffffff },
	{ ZORRO_PROD_GVP_COMBO_030_R4_SCSI,	~0x01ffffff },
	{ ZORRO_PROD_GVP_A1291,			~0x07ffffff },
	{ ZORRO_PROD_GVP_GFORCE_040_SCSI_1,	~0x07ffffff },
	{ 0 }
};
MODULE_DEVICE_TABLE(zorro, gvp11_zorro_tbl);

static struct zorro_driver gvp11_driver = {
	.name		= "gvp11",
	.id_table	= gvp11_zorro_tbl,
	.probe		= gvp11_probe,
	.remove		= __devexit_p(gvp11_remove),
};

static int __init gvp11_init(void)
{
	return zorro_register_driver(&gvp11_driver);
}
module_init(gvp11_init);

static void __exit gvp11_exit(void)
{
	zorro_unregister_driver(&gvp11_driver);
}
module_exit(gvp11_exit);

MODULE_DESCRIPTION("GVP Series II SCSI");
MODULE_LICENSE("GPL");
