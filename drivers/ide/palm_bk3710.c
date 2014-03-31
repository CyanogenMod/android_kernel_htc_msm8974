/*
 * Palmchip bk3710 IDE controller
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software, Inc., <source@mvista.com>
 *
 * ----------------------------------------------------------------------------
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/ide.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>

#define IDE_PALM_ATA_PRI_REG_OFFSET 0x1F0

#define IDE_PALM_ATA_PRI_CTL_OFFSET 0x3F6

#define BK3710_BMICP		0x00
#define BK3710_BMISP		0x02
#define BK3710_BMIDTP		0x04
#define BK3710_IDETIMP		0x40
#define BK3710_IDESTATUS	0x47
#define BK3710_UDMACTL		0x48
#define BK3710_MISCCTL		0x50
#define BK3710_REGSTB		0x54
#define BK3710_REGRCVR		0x58
#define BK3710_DATSTB		0x5C
#define BK3710_DATRCVR		0x60
#define BK3710_DMASTB		0x64
#define BK3710_DMARCVR		0x68
#define BK3710_UDMASTB		0x6C
#define BK3710_UDMATRP		0x70
#define BK3710_UDMAENV		0x74
#define BK3710_IORDYTMP		0x78

static unsigned ideclk_period; 

struct palm_bk3710_udmatiming {
	unsigned int rptime;	
	unsigned int cycletime;	
				
};

static const struct palm_bk3710_udmatiming palm_bk3710_udmatimings[6] = {
	{ 160, 240 / 2 },	
	{ 125, 160 / 2 },	
	{ 100, 120 / 2 },	
	{ 100,  90 / 2 },	
	{ 100,  60 / 2 },	
	{  85,  40 / 2 },	
};

static void palm_bk3710_setudmamode(void __iomem *base, unsigned int dev,
				    unsigned int mode)
{
	u8 tenv, trp, t0;
	u32 val32;
	u16 val16;

	
	t0 = DIV_ROUND_UP(palm_bk3710_udmatimings[mode].cycletime,
			  ideclk_period) - 1;
	tenv = DIV_ROUND_UP(20, ideclk_period) - 1;
	trp = DIV_ROUND_UP(palm_bk3710_udmatimings[mode].rptime,
			   ideclk_period) - 1;

	
	val32 = readl(base + BK3710_UDMASTB) & (0xFF << (dev ? 0 : 8));
	val32 |= (t0 << (dev ? 8 : 0));
	writel(val32, base + BK3710_UDMASTB);

	
	val32 = readl(base + BK3710_UDMATRP) & (0xFF << (dev ? 0 : 8));
	val32 |= (trp << (dev ? 8 : 0));
	writel(val32, base + BK3710_UDMATRP);

	
	val32 = readl(base + BK3710_UDMAENV) & (0xFF << (dev ? 0 : 8));
	val32 |= (tenv << (dev ? 8 : 0));
	writel(val32, base + BK3710_UDMAENV);

	
	val16 = readw(base + BK3710_UDMACTL) | (1 << dev);
	writew(val16, base + BK3710_UDMACTL);
}

static void palm_bk3710_setdmamode(void __iomem *base, unsigned int dev,
				   unsigned short min_cycle,
				   unsigned int mode)
{
	u8 td, tkw, t0;
	u32 val32;
	u16 val16;
	struct ide_timing *t;
	int cycletime;

	t = ide_timing_find_mode(mode);
	cycletime = max_t(int, t->cycle, min_cycle);

	
	t0 = DIV_ROUND_UP(cycletime, ideclk_period);
	td = DIV_ROUND_UP(t->active, ideclk_period);
	tkw = t0 - td - 1;
	td -= 1;

	val32 = readl(base + BK3710_DMASTB) & (0xFF << (dev ? 0 : 8));
	val32 |= (td << (dev ? 8 : 0));
	writel(val32, base + BK3710_DMASTB);

	val32 = readl(base + BK3710_DMARCVR) & (0xFF << (dev ? 0 : 8));
	val32 |= (tkw << (dev ? 8 : 0));
	writel(val32, base + BK3710_DMARCVR);

	
	val16 = readw(base + BK3710_UDMACTL) & ~(1 << dev);
	writew(val16, base + BK3710_UDMACTL);
}

static void palm_bk3710_setpiomode(void __iomem *base, ide_drive_t *mate,
				   unsigned int dev, unsigned int cycletime,
				   unsigned int mode)
{
	u8 t2, t2i, t0;
	u32 val32;
	struct ide_timing *t;

	t = ide_timing_find_mode(XFER_PIO_0 + mode);

	
	t0 = DIV_ROUND_UP(cycletime, ideclk_period);
	t2 = DIV_ROUND_UP(t->active, ideclk_period);

	t2i = t0 - t2 - 1;
	t2 -= 1;

	val32 = readl(base + BK3710_DATSTB) & (0xFF << (dev ? 0 : 8));
	val32 |= (t2 << (dev ? 8 : 0));
	writel(val32, base + BK3710_DATSTB);

	val32 = readl(base + BK3710_DATRCVR) & (0xFF << (dev ? 0 : 8));
	val32 |= (t2i << (dev ? 8 : 0));
	writel(val32, base + BK3710_DATRCVR);

	if (mate) {
		u8 mode2 = mate->pio_mode - XFER_PIO_0;

		if (mode2 < mode)
			mode = mode2;
	}

	
	t0 = DIV_ROUND_UP(t->cyc8b, ideclk_period);
	t2 = DIV_ROUND_UP(t->act8b, ideclk_period);

	t2i = t0 - t2 - 1;
	t2 -= 1;

	val32 = readl(base + BK3710_REGSTB) & (0xFF << (dev ? 0 : 8));
	val32 |= (t2 << (dev ? 8 : 0));
	writel(val32, base + BK3710_REGSTB);

	val32 = readl(base + BK3710_REGRCVR) & (0xFF << (dev ? 0 : 8));
	val32 |= (t2i << (dev ? 8 : 0));
	writel(val32, base + BK3710_REGRCVR);
}

static void palm_bk3710_set_dma_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
	int is_slave = drive->dn & 1;
	void __iomem *base = (void *)hwif->dma_base;
	const u8 xferspeed = drive->dma_mode;

	if (xferspeed >= XFER_UDMA_0) {
		palm_bk3710_setudmamode(base, is_slave,
					xferspeed - XFER_UDMA_0);
	} else {
		palm_bk3710_setdmamode(base, is_slave,
				       drive->id[ATA_ID_EIDE_DMA_MIN],
				       xferspeed);
	}
}

static void palm_bk3710_set_pio_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
	unsigned int cycle_time;
	int is_slave = drive->dn & 1;
	ide_drive_t *mate;
	void __iomem *base = (void *)hwif->dma_base;
	const u8 pio = drive->pio_mode - XFER_PIO_0;

	cycle_time = ide_pio_cycle_time(drive, pio);
	mate = ide_get_pair_dev(drive);
	palm_bk3710_setpiomode(base, mate, is_slave, cycle_time, pio);
}

static void __devinit palm_bk3710_chipinit(void __iomem *base)
{

	writew(BIT(15), base + BK3710_IDETIMP);

	writew(0, base + BK3710_UDMACTL);

	writel(0x001, base + BK3710_MISCCTL);

	writel(0xFFFF, base + BK3710_IORDYTMP);

	writew(0, base + BK3710_BMISP);

	palm_bk3710_setpiomode(base, NULL, 0, 600, 0);
	palm_bk3710_setpiomode(base, NULL, 1, 600, 0);
}

static u8 palm_bk3710_cable_detect(ide_hwif_t *hwif)
{
	return ATA_CBL_PATA80;
}

static int __devinit palm_bk3710_init_dma(ide_hwif_t *hwif,
					  const struct ide_port_info *d)
{
	printk(KERN_INFO "    %s: MMIO-DMA\n", hwif->name);

	if (ide_allocate_dma_engine(hwif))
		return -1;

	hwif->dma_base = hwif->io_ports.data_addr - IDE_PALM_ATA_PRI_REG_OFFSET;

	return 0;
}

static const struct ide_port_ops palm_bk3710_ports_ops = {
	.set_pio_mode		= palm_bk3710_set_pio_mode,
	.set_dma_mode		= palm_bk3710_set_dma_mode,
	.cable_detect		= palm_bk3710_cable_detect,
};

static struct ide_port_info __devinitdata palm_bk3710_port_info = {
	.init_dma		= palm_bk3710_init_dma,
	.port_ops		= &palm_bk3710_ports_ops,
	.dma_ops		= &sff_dma_ops,
	.host_flags		= IDE_HFLAG_MMIO,
	.pio_mask		= ATA_PIO4,
	.mwdma_mask		= ATA_MWDMA2,
	.chipset		= ide_palm3710,
};

static int __init palm_bk3710_probe(struct platform_device *pdev)
{
	struct clk *clk;
	struct resource *mem, *irq;
	void __iomem *base;
	unsigned long rate, mem_size;
	int i, rc;
	struct ide_hw hw, *hws[] = { &hw };

	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk))
		return -ENODEV;

	clk_enable(clk);
	rate = clk_get_rate(clk);

	
	ideclk_period = 1000000000UL / rate;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		printk(KERN_ERR "failed to get memory region resource\n");
		return -ENODEV;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq == NULL) {
		printk(KERN_ERR "failed to get IRQ resource\n");
		return -ENODEV;
	}

	mem_size = resource_size(mem);
	if (request_mem_region(mem->start, mem_size, "palm_bk3710") == NULL) {
		printk(KERN_ERR "failed to request memory region\n");
		return -EBUSY;
	}

	base = ioremap(mem->start, mem_size);
	if (!base) {
		printk(KERN_ERR "failed to map IO memory\n");
		release_mem_region(mem->start, mem_size);
		return -ENOMEM;
	}

	
	palm_bk3710_chipinit(base);

	memset(&hw, 0, sizeof(hw));
	for (i = 0; i < IDE_NR_PORTS - 2; i++)
		hw.io_ports_array[i] = (unsigned long)
				(base + IDE_PALM_ATA_PRI_REG_OFFSET + i);
	hw.io_ports.ctl_addr = (unsigned long)
			(base + IDE_PALM_ATA_PRI_CTL_OFFSET);
	hw.irq = irq->start;
	hw.dev = &pdev->dev;

	palm_bk3710_port_info.udma_mask = rate < 100000000 ? ATA_UDMA4 :
							     ATA_UDMA5;

	
	rc = ide_host_add(&palm_bk3710_port_info, hws, 1, NULL);
	if (rc)
		goto out;

	return 0;
out:
	printk(KERN_WARNING "Palm Chip BK3710 IDE Register Fail\n");
	return rc;
}

MODULE_ALIAS("platform:palm_bk3710");

static struct platform_driver platform_bk_driver = {
	.driver = {
		.name = "palm_bk3710",
		.owner = THIS_MODULE,
	},
};

static int __init palm_bk3710_init(void)
{
	return platform_driver_probe(&platform_bk_driver, palm_bk3710_probe);
}

module_init(palm_bk3710_init);
MODULE_LICENSE("GPL");
