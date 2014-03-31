#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/ide.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>


int config_drive_for_dma(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	u16 *id = drive->id;

	if (drive->media != ide_disk) {
		if (hwif->host_flags & IDE_HFLAG_NO_ATAPI_DMA)
			return 0;
	}

	if ((id[ATA_ID_FIELD_VALID] & 4) &&
	    ((id[ATA_ID_UDMA_MODES] >> 8) & 0x7f))
		return 1;

	if ((id[ATA_ID_MWDMA_MODES] & 0x404) == 0x404 ||
	    (id[ATA_ID_SWDMA_MODES] & 0x404) == 0x404)
		return 1;

	
	if (ide_dma_good_drive(drive))
		return 1;

	return 0;
}

u8 ide_dma_sff_read_status(ide_hwif_t *hwif)
{
	unsigned long addr = hwif->dma_base + ATA_DMA_STATUS;

	if (hwif->host_flags & IDE_HFLAG_MMIO)
		return readb((void __iomem *)addr);
	else
		return inb(addr);
}
EXPORT_SYMBOL_GPL(ide_dma_sff_read_status);

static void ide_dma_sff_write_status(ide_hwif_t *hwif, u8 val)
{
	unsigned long addr = hwif->dma_base + ATA_DMA_STATUS;

	if (hwif->host_flags & IDE_HFLAG_MMIO)
		writeb(val, (void __iomem *)addr);
	else
		outb(val, addr);
}


void ide_dma_host_set(ide_drive_t *drive, int on)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 unit = drive->dn & 1;
	u8 dma_stat = hwif->dma_ops->dma_sff_read_status(hwif);

	if (on)
		dma_stat |= (1 << (5 + unit));
	else
		dma_stat &= ~(1 << (5 + unit));

	ide_dma_sff_write_status(hwif, dma_stat);
}
EXPORT_SYMBOL_GPL(ide_dma_host_set);


int ide_build_dmatable(ide_drive_t *drive, struct ide_cmd *cmd)
{
	ide_hwif_t *hwif = drive->hwif;
	__le32 *table = (__le32 *)hwif->dmatable_cpu;
	unsigned int count = 0;
	int i;
	struct scatterlist *sg;
	u8 is_trm290 = !!(hwif->host_flags & IDE_HFLAG_TRM290);

	for_each_sg(hwif->sg_table, sg, cmd->sg_nents, i) {
		u32 cur_addr, cur_len, xcount, bcount;

		cur_addr = sg_dma_address(sg);
		cur_len = sg_dma_len(sg);


		while (cur_len) {
			if (count++ >= PRD_ENTRIES)
				goto use_pio_instead;

			bcount = 0x10000 - (cur_addr & 0xffff);
			if (bcount > cur_len)
				bcount = cur_len;
			*table++ = cpu_to_le32(cur_addr);
			xcount = bcount & 0xffff;
			if (is_trm290)
				xcount = ((xcount >> 2) - 1) << 16;
			else if (xcount == 0x0000) {
				if (count++ >= PRD_ENTRIES)
					goto use_pio_instead;
				*table++ = cpu_to_le32(0x8000);
				*table++ = cpu_to_le32(cur_addr + 0x8000);
				xcount = 0x8000;
			}
			*table++ = cpu_to_le32(xcount);
			cur_addr += bcount;
			cur_len -= bcount;
		}
	}

	if (count) {
		if (!is_trm290)
			*--table |= cpu_to_le32(0x80000000);
		return count;
	}

use_pio_instead:
	printk(KERN_ERR "%s: %s\n", drive->name,
		count ? "DMA table too small" : "empty DMA table?");

	return 0; 
}
EXPORT_SYMBOL_GPL(ide_build_dmatable);


int ide_dma_setup(ide_drive_t *drive, struct ide_cmd *cmd)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 mmio = (hwif->host_flags & IDE_HFLAG_MMIO) ? 1 : 0;
	u8 rw = (cmd->tf_flags & IDE_TFLAG_WRITE) ? 0 : ATA_DMA_WR;
	u8 dma_stat;

	
	if (ide_build_dmatable(drive, cmd) == 0) {
		ide_map_sg(drive, cmd);
		return 1;
	}

	
	if (mmio)
		writel(hwif->dmatable_dma,
		       (void __iomem *)(hwif->dma_base + ATA_DMA_TABLE_OFS));
	else
		outl(hwif->dmatable_dma, hwif->dma_base + ATA_DMA_TABLE_OFS);

	
	if (mmio)
		writeb(rw, (void __iomem *)(hwif->dma_base + ATA_DMA_CMD));
	else
		outb(rw, hwif->dma_base + ATA_DMA_CMD);

	
	dma_stat = hwif->dma_ops->dma_sff_read_status(hwif);

	
	ide_dma_sff_write_status(hwif, dma_stat | ATA_DMA_ERR | ATA_DMA_INTR);

	return 0;
}
EXPORT_SYMBOL_GPL(ide_dma_setup);


int ide_dma_sff_timer_expiry(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 dma_stat = hwif->dma_ops->dma_sff_read_status(hwif);

	printk(KERN_WARNING "%s: %s: DMA status (0x%02x)\n",
		drive->name, __func__, dma_stat);

	if ((dma_stat & 0x18) == 0x18)	
		return WAIT_CMD;

	hwif->expiry = NULL;	

	if (dma_stat & ATA_DMA_ERR)	
		return -1;

	if (dma_stat & ATA_DMA_ACTIVE)	
		return WAIT_CMD;

	if (dma_stat & ATA_DMA_INTR)	
		return WAIT_CMD;

	return 0;	
}
EXPORT_SYMBOL_GPL(ide_dma_sff_timer_expiry);

void ide_dma_start(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 dma_cmd;

	if (hwif->host_flags & IDE_HFLAG_MMIO) {
		dma_cmd = readb((void __iomem *)(hwif->dma_base + ATA_DMA_CMD));
		writeb(dma_cmd | ATA_DMA_START,
		       (void __iomem *)(hwif->dma_base + ATA_DMA_CMD));
	} else {
		dma_cmd = inb(hwif->dma_base + ATA_DMA_CMD);
		outb(dma_cmd | ATA_DMA_START, hwif->dma_base + ATA_DMA_CMD);
	}
}
EXPORT_SYMBOL_GPL(ide_dma_start);

int ide_dma_end(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 dma_stat = 0, dma_cmd = 0;

	
	if (hwif->host_flags & IDE_HFLAG_MMIO) {
		dma_cmd = readb((void __iomem *)(hwif->dma_base + ATA_DMA_CMD));
		writeb(dma_cmd & ~ATA_DMA_START,
		       (void __iomem *)(hwif->dma_base + ATA_DMA_CMD));
	} else {
		dma_cmd = inb(hwif->dma_base + ATA_DMA_CMD);
		outb(dma_cmd & ~ATA_DMA_START, hwif->dma_base + ATA_DMA_CMD);
	}

	
	dma_stat = hwif->dma_ops->dma_sff_read_status(hwif);

	
	ide_dma_sff_write_status(hwif, dma_stat | ATA_DMA_ERR | ATA_DMA_INTR);

#define CHECK_DMA_MASK (ATA_DMA_ACTIVE | ATA_DMA_ERR | ATA_DMA_INTR)

	
	if ((dma_stat & CHECK_DMA_MASK) != ATA_DMA_INTR)
		return 0x10 | dma_stat;
	return 0;
}
EXPORT_SYMBOL_GPL(ide_dma_end);

int ide_dma_test_irq(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 dma_stat = hwif->dma_ops->dma_sff_read_status(hwif);

	return (dma_stat & ATA_DMA_INTR) ? 1 : 0;
}
EXPORT_SYMBOL_GPL(ide_dma_test_irq);

const struct ide_dma_ops sff_dma_ops = {
	.dma_host_set		= ide_dma_host_set,
	.dma_setup		= ide_dma_setup,
	.dma_start		= ide_dma_start,
	.dma_end		= ide_dma_end,
	.dma_test_irq		= ide_dma_test_irq,
	.dma_lost_irq		= ide_dma_lost_irq,
	.dma_timer_expiry	= ide_dma_sff_timer_expiry,
	.dma_sff_read_status	= ide_dma_sff_read_status,
};
EXPORT_SYMBOL_GPL(sff_dma_ops);
