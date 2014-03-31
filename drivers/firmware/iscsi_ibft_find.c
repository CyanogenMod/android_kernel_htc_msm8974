/*
 *  Copyright 2007-2010 Red Hat, Inc.
 *  by Peter Jones <pjones@redhat.com>
 *  Copyright 2007 IBM, Inc.
 *  by Konrad Rzeszutek <konradr@linux.vnet.ibm.com>
 *  Copyright 2008
 *  by Konrad Rzeszutek <ketuzsezr@darnok.org>
 *
 * This code finds the iSCSI Boot Format Table.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2.0 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bootmem.h>
#include <linux/blkdev.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/efi.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/acpi.h>
#include <linux/iscsi_ibft.h>

#include <asm/mmzone.h>

struct acpi_table_ibft *ibft_addr;
EXPORT_SYMBOL_GPL(ibft_addr);

static const struct {
	char *sign;
} ibft_signs[] = {
	{ "iBFT" },
	{ "BIFT" },	
};

#define IBFT_SIGN_LEN 4
#define IBFT_START 0x80000 
#define IBFT_END 0x100000 
#define VGA_MEM 0xA0000 
#define VGA_SIZE 0x20000 

static int __init find_ibft_in_mem(void)
{
	unsigned long pos;
	unsigned int len = 0;
	void *virt;
	int i;

	for (pos = IBFT_START; pos < IBFT_END; pos += 16) {
		if (pos == VGA_MEM)
			pos += VGA_SIZE;
		virt = isa_bus_to_virt(pos);

		for (i = 0; i < ARRAY_SIZE(ibft_signs); i++) {
			if (memcmp(virt, ibft_signs[i].sign, IBFT_SIGN_LEN) ==
			    0) {
				unsigned long *addr =
				    (unsigned long *)isa_bus_to_virt(pos + 4);
				len = *addr;
				if (pos + len <= (IBFT_END-1)) {
					ibft_addr = (struct acpi_table_ibft *)virt;
					pr_info("iBFT found at 0x%lx.\n", pos);
					goto done;
				}
			}
		}
	}
done:
	return len;
}
unsigned long __init find_ibft_region(unsigned long *sizep)
{
	ibft_addr = NULL;


	if (!efi_enabled)
		find_ibft_in_mem();

	if (ibft_addr) {
		*sizep = PAGE_ALIGN(ibft_addr->header.length);
		return (u64)isa_virt_to_bus(ibft_addr);
	}

	*sizep = 0;
	return 0;
}
