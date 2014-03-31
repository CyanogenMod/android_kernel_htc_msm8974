/*
 *  Copyright (C) 2000-2002	Andre Hedrick <andre@linux-ide.org>
 *  Copyright (C) 2003		Red Hat
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/blkpg.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/bitops.h>
#include <linux/nmi.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>

void SELECT_MASK(ide_drive_t *drive, int mask)
{
	const struct ide_port_ops *port_ops = drive->hwif->port_ops;

	if (port_ops && port_ops->maskproc)
		port_ops->maskproc(drive, mask);
}

u8 ide_read_error(ide_drive_t *drive)
{
	struct ide_taskfile tf;

	drive->hwif->tp_ops->tf_read(drive, &tf, IDE_VALID_ERROR);

	return tf.error;
}
EXPORT_SYMBOL_GPL(ide_read_error);

void ide_fix_driveid(u16 *id)
{
#ifndef __LITTLE_ENDIAN
# ifdef __BIG_ENDIAN
	int i;

	for (i = 0; i < 256; i++)
		id[i] = __le16_to_cpu(id[i]);
# else
#  error "Please fix <asm/byteorder.h>"
# endif
#endif
}


void ide_fixstring(u8 *s, const int bytecount, const int byteswap)
{
	u8 *p, *end = &s[bytecount & ~1]; 

	if (byteswap) {
		
		for (p = s ; p != end ; p += 2)
			be16_to_cpus((u16 *) p);
	}

	
	p = s;
	while (s != end && *s == ' ')
		++s;
	
	while (s != end && *s) {
		if (*s++ != ' ' || (s != end && *s && *s != ' '))
			*p++ = *(s-1);
	}
	
	while (p != end)
		*p++ = '\0';
}
EXPORT_SYMBOL(ide_fixstring);

int __ide_wait_stat(ide_drive_t *drive, u8 good, u8 bad,
		    unsigned long timeout, u8 *rstat)
{
	ide_hwif_t *hwif = drive->hwif;
	const struct ide_tp_ops *tp_ops = hwif->tp_ops;
	unsigned long flags;
	int i;
	u8 stat;

	udelay(1);	
	stat = tp_ops->read_status(hwif);

	if (stat & ATA_BUSY) {
		local_save_flags(flags);
		local_irq_enable_in_hardirq();
		timeout += jiffies;
		while ((stat = tp_ops->read_status(hwif)) & ATA_BUSY) {
			if (time_after(jiffies, timeout)) {
				stat = tp_ops->read_status(hwif);
				if ((stat & ATA_BUSY) == 0)
					break;

				local_irq_restore(flags);
				*rstat = stat;
				return -EBUSY;
			}
		}
		local_irq_restore(flags);
	}
	for (i = 0; i < 10; i++) {
		udelay(1);
		stat = tp_ops->read_status(hwif);

		if (OK_STAT(stat, good, bad)) {
			*rstat = stat;
			return 0;
		}
	}
	*rstat = stat;
	return -EFAULT;
}

int ide_wait_stat(ide_startstop_t *startstop, ide_drive_t *drive, u8 good,
		  u8 bad, unsigned long timeout)
{
	int err;
	u8 stat;

	
	if (drive->max_failures && (drive->failures > drive->max_failures)) {
		*startstop = ide_stopped;
		return 1;
	}

	err = __ide_wait_stat(drive, good, bad, timeout, &stat);

	if (err) {
		char *s = (err == -EBUSY) ? "status timeout" : "status error";
		*startstop = ide_error(drive, s, stat);
	}

	return err;
}
EXPORT_SYMBOL(ide_wait_stat);


int ide_in_drive_list(u16 *id, const struct drive_list_entry *table)
{
	for ( ; table->id_model; table++)
		if ((!strcmp(table->id_model, (char *)&id[ATA_ID_PROD])) &&
		    (!table->id_firmware ||
		     strstr((char *)&id[ATA_ID_FW_REV], table->id_firmware)))
			return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(ide_in_drive_list);

static const struct drive_list_entry ivb_list[] = {
	{ "QUANTUM FIREBALLlct10 05"	, "A03.0900"	},
	{ "QUANTUM FIREBALLlct20 30"	, "APL.0900"	},
	{ "TSSTcorp CDDVDW SH-S202J"	, "SB00"	},
	{ "TSSTcorp CDDVDW SH-S202J"	, "SB01"	},
	{ "TSSTcorp CDDVDW SH-S202N"	, "SB00"	},
	{ "TSSTcorp CDDVDW SH-S202N"	, "SB01"	},
	{ "TSSTcorp CDDVDW SH-S202H"	, "SB00"	},
	{ "TSSTcorp CDDVDW SH-S202H"	, "SB01"	},
	{ "SAMSUNG SP0822N"		, "WA100-10"	},
	{ NULL				, NULL		}
};

u8 eighty_ninty_three(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	u16 *id = drive->id;
	int ivb = ide_in_drive_list(id, ivb_list);

	if (hwif->cbl == ATA_CBL_SATA || hwif->cbl == ATA_CBL_PATA40_SHORT)
		return 1;

	if (ivb)
		printk(KERN_DEBUG "%s: skipping word 93 validity check\n",
				  drive->name);

	if (ata_id_is_sata(id) && !ivb)
		return 1;

	if (hwif->cbl != ATA_CBL_PATA80 && !ivb)
		goto no_80w;

	if (id[ATA_ID_HW_CONFIG] & 0x4000)
		return 1;

	if (ivb) {
		const char *model = (char *)&id[ATA_ID_PROD];

		if (strstr(model, "TSSTcorp CDDVDW SH-S202")) {
			if (hwif->cbl == ATA_CBL_PATA80)
				return 1;
		} else {
			
			if (id[ATA_ID_HW_CONFIG] & 0x2000)
				return 1;
		}
	}
no_80w:
	if (drive->dev_flags & IDE_DFLAG_UDMA33_WARNED)
		return 0;

	printk(KERN_WARNING "%s: %s side 80-wire cable detection failed, "
			    "limiting max speed to UDMA33\n",
			    drive->name,
			    hwif->cbl == ATA_CBL_PATA80 ? "drive" : "host");

	drive->dev_flags |= IDE_DFLAG_UDMA33_WARNED;

	return 0;
}

static const char *nien_quirk_list[] = {
	"QUANTUM FIREBALLlct08 08",
	"QUANTUM FIREBALLP KA6.4",
	"QUANTUM FIREBALLP KA9.1",
	"QUANTUM FIREBALLP KX13.6",
	"QUANTUM FIREBALLP KX20.5",
	"QUANTUM FIREBALLP KX27.3",
	"QUANTUM FIREBALLP LM20.4",
	"QUANTUM FIREBALLP LM20.5",
	"FUJITSU MHZ2160BH G2",
	NULL
};

void ide_check_nien_quirk_list(ide_drive_t *drive)
{
	const char **list, *m = (char *)&drive->id[ATA_ID_PROD];

	for (list = nien_quirk_list; *list != NULL; list++)
		if (strstr(m, *list) != NULL) {
			drive->dev_flags |= IDE_DFLAG_NIEN_QUIRK;
			return;
		}
}

int ide_driveid_update(ide_drive_t *drive)
{
	u16 *id;
	int rc;

	id = kmalloc(SECTOR_SIZE, GFP_ATOMIC);
	if (id == NULL)
		return 0;

	SELECT_MASK(drive, 1);
	rc = ide_dev_read_id(drive, ATA_CMD_ID_ATA, id, 1);
	SELECT_MASK(drive, 0);

	if (rc)
		goto out_err;

	drive->id[ATA_ID_UDMA_MODES]  = id[ATA_ID_UDMA_MODES];
	drive->id[ATA_ID_MWDMA_MODES] = id[ATA_ID_MWDMA_MODES];
	drive->id[ATA_ID_SWDMA_MODES] = id[ATA_ID_SWDMA_MODES];
	drive->id[ATA_ID_CFA_MODES]   = id[ATA_ID_CFA_MODES];
	

	kfree(id);

	return 1;
out_err:
	if (rc == 2)
		printk(KERN_ERR "%s: %s: bad status\n", drive->name, __func__);
	kfree(id);
	return 0;
}

int ide_config_drive_speed(ide_drive_t *drive, u8 speed)
{
	ide_hwif_t *hwif = drive->hwif;
	const struct ide_tp_ops *tp_ops = hwif->tp_ops;
	struct ide_taskfile tf;
	u16 *id = drive->id, i;
	int error = 0;
	u8 stat;

#ifdef CONFIG_BLK_DEV_IDEDMA
	if (hwif->dma_ops)	
		hwif->dma_ops->dma_host_set(drive, 0);
#endif

	
	if ((speed & 0xf8) == XFER_PIO_0 && ata_id_has_iordy(drive->id) == 0)
		goto skip;


	udelay(1);
	tp_ops->dev_select(drive);
	SELECT_MASK(drive, 1);
	udelay(1);
	tp_ops->write_devctl(hwif, ATA_NIEN | ATA_DEVCTL_OBS);

	memset(&tf, 0, sizeof(tf));
	tf.feature = SETFEATURES_XFER;
	tf.nsect   = speed;

	tp_ops->tf_load(drive, &tf, IDE_VALID_FEATURE | IDE_VALID_NSECT);

	tp_ops->exec_command(hwif, ATA_CMD_SET_FEATURES);

	if (drive->dev_flags & IDE_DFLAG_NIEN_QUIRK)
		tp_ops->write_devctl(hwif, ATA_DEVCTL_OBS);

	error = __ide_wait_stat(drive, drive->ready_stat,
				ATA_BUSY | ATA_DRQ | ATA_ERR,
				WAIT_CMD, &stat);

	SELECT_MASK(drive, 0);

	if (error) {
		(void) ide_dump_status(drive, "set_drive_speed_status", stat);
		return error;
	}

	if (speed >= XFER_SW_DMA_0) {
		id[ATA_ID_UDMA_MODES]  &= ~0xFF00;
		id[ATA_ID_MWDMA_MODES] &= ~0x0700;
		id[ATA_ID_SWDMA_MODES] &= ~0x0700;
		if (ata_id_is_cfa(id))
			id[ATA_ID_CFA_MODES] &= ~0x0E00;
	} else	if (ata_id_is_cfa(id))
		id[ATA_ID_CFA_MODES] &= ~0x01C0;

 skip:
#ifdef CONFIG_BLK_DEV_IDEDMA
	if (speed >= XFER_SW_DMA_0 && (drive->dev_flags & IDE_DFLAG_USING_DMA))
		hwif->dma_ops->dma_host_set(drive, 1);
	else if (hwif->dma_ops)	
		ide_dma_off_quietly(drive);
#endif

	if (speed >= XFER_UDMA_0) {
		i = 1 << (speed - XFER_UDMA_0);
		id[ATA_ID_UDMA_MODES] |= (i << 8 | i);
	} else if (ata_id_is_cfa(id) && speed >= XFER_MW_DMA_3) {
		i = speed - XFER_MW_DMA_2;
		id[ATA_ID_CFA_MODES] |= i << 9;
	} else if (speed >= XFER_MW_DMA_0) {
		i = 1 << (speed - XFER_MW_DMA_0);
		id[ATA_ID_MWDMA_MODES] |= (i << 8 | i);
	} else if (speed >= XFER_SW_DMA_0) {
		i = 1 << (speed - XFER_SW_DMA_0);
		id[ATA_ID_SWDMA_MODES] |= (i << 8 | i);
	} else if (ata_id_is_cfa(id) && speed >= XFER_PIO_5) {
		i = speed - XFER_PIO_4;
		id[ATA_ID_CFA_MODES] |= i << 6;
	}

	if (!drive->init_speed)
		drive->init_speed = speed;
	drive->current_speed = speed;
	return error;
}

void __ide_set_handler(ide_drive_t *drive, ide_handler_t *handler,
		       unsigned int timeout)
{
	ide_hwif_t *hwif = drive->hwif;

	BUG_ON(hwif->handler);
	hwif->handler		= handler;
	hwif->timer.expires	= jiffies + timeout;
	hwif->req_gen_timer	= hwif->req_gen;
	add_timer(&hwif->timer);
}

void ide_set_handler(ide_drive_t *drive, ide_handler_t *handler,
		     unsigned int timeout)
{
	ide_hwif_t *hwif = drive->hwif;
	unsigned long flags;

	spin_lock_irqsave(&hwif->lock, flags);
	__ide_set_handler(drive, handler, timeout);
	spin_unlock_irqrestore(&hwif->lock, flags);
}
EXPORT_SYMBOL(ide_set_handler);


void ide_execute_command(ide_drive_t *drive, struct ide_cmd *cmd,
			 ide_handler_t *handler, unsigned timeout)
{
	ide_hwif_t *hwif = drive->hwif;
	unsigned long flags;

	spin_lock_irqsave(&hwif->lock, flags);
	if ((cmd->protocol != ATAPI_PROT_DMA &&
	     cmd->protocol != ATAPI_PROT_PIO) ||
	    (drive->atapi_flags & IDE_AFLAG_DRQ_INTERRUPT))
		__ide_set_handler(drive, handler, timeout);
	hwif->tp_ops->exec_command(hwif, cmd->tf.command);
	ndelay(400);
	spin_unlock_irqrestore(&hwif->lock, flags);
}

int ide_wait_not_busy(ide_hwif_t *hwif, unsigned long timeout)
{
	u8 stat = 0;

	while (timeout--) {
		mdelay(1);
		stat = hwif->tp_ops->read_status(hwif);
		if ((stat & ATA_BUSY) == 0)
			return 0;
		if (stat == 0xff)
			return -ENODEV;
		touch_softlockup_watchdog();
		touch_nmi_watchdog();
	}
	return -EBUSY;
}
