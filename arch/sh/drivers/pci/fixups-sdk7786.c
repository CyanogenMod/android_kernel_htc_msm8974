/*
 * SDK7786 FPGA PCIe mux handling
 *
 * Copyright (C) 2010  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#define pr_fmt(fmt) "PCI: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <mach/fpga.h>

static unsigned int slot4en __devinitdata;

char *__devinit pcibios_setup(char *str)
{
	if (strcmp(str, "slot4en") == 0) {
		slot4en = 1;
		return NULL;
	}

	return str;
}

static int __init sdk7786_pci_init(void)
{
	u16 data = fpga_read_reg(PCIECR);

	slot4en ?: (!(data & PCIECR_PRST4) && (data & PCIECR_PRST3));
	if (slot4en) {
		pr_info("Activating PCIe slot#4 (disabling slot#3)\n");

		data &= ~PCIECR_PCIEMUX1;
		fpga_write_reg(data, PCIECR);

		
		if ((data & PCIECR_PRST3) == 0) {
			pr_warning("Unreachable card detected in slot#3\n");
			return -EBUSY;
		}
	} else
		pr_info("PCIe slot#4 disabled\n");

	return 0;
}
postcore_initcall(sdk7786_pci_init);
