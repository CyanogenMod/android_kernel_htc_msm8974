/*
 * sh7724 MMCIF loader
 *
 * Copyright (C) 2010 Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/mmc/sh_mmcif.h>
#include <linux/mmc/boot.h>
#include <mach/romimage.h>

#define MMCIF_BASE      (void __iomem *)0xa4ca0000

#define MSTPCR2		0xa4150038
#define PTWCR		0xa4050146
#define PTXCR		0xa4050148
#define PSELA		0xa405014e
#define PSELE		0xa4050156
#define HIZCRC		0xa405015c
#define DRVCRA		0xa405018a

asmlinkage void mmcif_loader(unsigned char *buf, unsigned long no_bytes)
{
	mmcif_update_progress(MMC_PROGRESS_ENTER);

	
	__raw_writel(__raw_readl(MSTPCR2) & ~0x20000000, MSTPCR2);

	
	__raw_writew(0x0000, PTWCR);

	
	__raw_writew(__raw_readw(PTXCR) & ~0x000f, PTXCR);

	
	__raw_writew(__raw_readw(PSELA) & ~0x2000, PSELA);

	
	__raw_writew(__raw_readw(PSELE) & ~0x3000, PSELE);

	
	__raw_writew(__raw_readw(HIZCRC) & ~0x0620, HIZCRC);

	
	__raw_writew(__raw_readw(DRVCRA) | 0x3000, DRVCRA);

	mmcif_update_progress(MMC_PROGRESS_INIT);

	
	sh_mmcif_boot_init(MMCIF_BASE);

	mmcif_update_progress(MMC_PROGRESS_LOAD);

	
	sh_mmcif_boot_do_read(MMCIF_BASE, 512,
	                      (no_bytes + SH_MMCIF_BBS - 1) / SH_MMCIF_BBS,
			      buf);

	
	__raw_writel(__raw_readl(MSTPCR2) | 0x20000000, MSTPCR2);

	mmcif_update_progress(MMC_PROGRESS_DONE);
}
