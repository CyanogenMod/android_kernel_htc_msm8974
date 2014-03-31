/*
 * SDK7786 FPGA Support.
 *
 * Copyright (C) 2010  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/bcd.h>
#include <mach/fpga.h>
#include <asm/sizes.h>

#define FPGA_REGS_OFFSET	0x03fff800
#define FPGA_REGS_SIZE		0x490

static void __iomem *sdk7786_fpga_probe(void)
{
	unsigned long area;
	void __iomem *base;

	for (area = PA_AREA0; area < PA_AREA7; area += SZ_64M) {
		base = ioremap_nocache(area + FPGA_REGS_OFFSET, FPGA_REGS_SIZE);
		if (!base) {
			
			continue;
		}

		if (ioread16(base + SRSTR) == SRSTR_MAGIC)
			return base;	

		iounmap(base);
	}

	return NULL;
}

void __iomem *sdk7786_fpga_base;

void __init sdk7786_fpga_init(void)
{
	u16 version, date;

	sdk7786_fpga_base = sdk7786_fpga_probe();
	if (unlikely(!sdk7786_fpga_base)) {
		panic("FPGA detection failed.\n");
		return;
	}

	version = fpga_read_reg(FPGAVR);
	date = fpga_read_reg(FPGADR);

	pr_info("\tFPGA version:\t%d.%d (built on %d/%d/%d)\n",
		bcd2bin(version >> 8) & 0xf, bcd2bin(version & 0xf),
		((date >> 12) & 0xf) + 2000,
		(date >> 8) & 0xf, bcd2bin(date & 0xff));
}
