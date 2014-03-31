/*
 * arch/arm/mach-ixp4xx/common-pci.c 
 *
 * IXP4XX PCI routines for all platforms
 *
 * Maintainer: Deepak Saxena <dsaxena@plexity.net>
 *
 * Copyright (C) 2002 Intel Corporation.
 * Copyright (C) 2003 Greg Ungerer <gerg@snapgear.com>
 * Copyright (C) 2003-2004 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/export.h>
#include <asm/dma-mapping.h>

#include <asm/cputype.h>
#include <asm/irq.h>
#include <asm/sizes.h>
#include <asm/mach/pci.h>
#include <mach/hardware.h>


int (*ixp4xx_pci_read)(u32 addr, u32 cmd, u32* data);

unsigned long ixp4xx_pci_reg_base = 0;

static DEFINE_RAW_SPINLOCK(ixp4xx_pci_lock);

static void crp_read(u32 ad_cbe, u32 *data)
{
	unsigned long flags;
	raw_spin_lock_irqsave(&ixp4xx_pci_lock, flags);
	*PCI_CRP_AD_CBE = ad_cbe;
	*data = *PCI_CRP_RDATA;
	raw_spin_unlock_irqrestore(&ixp4xx_pci_lock, flags);
}

static void crp_write(u32 ad_cbe, u32 data)
{ 
	unsigned long flags;
	raw_spin_lock_irqsave(&ixp4xx_pci_lock, flags);
	*PCI_CRP_AD_CBE = CRP_AD_CBE_WRITE | ad_cbe;
	*PCI_CRP_WDATA = data;
	raw_spin_unlock_irqrestore(&ixp4xx_pci_lock, flags);
}

static inline int check_master_abort(void)
{
	
	unsigned long isr = *PCI_ISR;

	if (isr & PCI_ISR_PFE) {
		    
		*PCI_ISR = PCI_ISR_PFE;
		pr_debug("%s failed\n", __func__);
		return 1;
	}

	return 0;
}

int ixp4xx_pci_read_errata(u32 addr, u32 cmd, u32* data)
{
	unsigned long flags;
	int retval = 0;
	int i;

	raw_spin_lock_irqsave(&ixp4xx_pci_lock, flags);

	*PCI_NP_AD = addr;

	for (i = 0; i < 8; i++) {
		*PCI_NP_CBE = cmd;
		*data = *PCI_NP_RDATA;
		*data = *PCI_NP_RDATA;
	}

	if(check_master_abort())
		retval = 1;

	raw_spin_unlock_irqrestore(&ixp4xx_pci_lock, flags);
	return retval;
}

int ixp4xx_pci_read_no_errata(u32 addr, u32 cmd, u32* data)
{
	unsigned long flags;
	int retval = 0;

	raw_spin_lock_irqsave(&ixp4xx_pci_lock, flags);

	*PCI_NP_AD = addr;

	    
	*PCI_NP_CBE = cmd;

	
	*data = *PCI_NP_RDATA; 

	if(check_master_abort())
		retval = 1;

	raw_spin_unlock_irqrestore(&ixp4xx_pci_lock, flags);
	return retval;
}

int ixp4xx_pci_write(u32 addr, u32 cmd, u32 data)
{    
	unsigned long flags;
	int retval = 0;

	raw_spin_lock_irqsave(&ixp4xx_pci_lock, flags);

	*PCI_NP_AD = addr;

	
	*PCI_NP_CBE = cmd;

	
	*PCI_NP_WDATA = data;

	if(check_master_abort())
		retval = 1;

	raw_spin_unlock_irqrestore(&ixp4xx_pci_lock, flags);
	return retval;
}

static u32 ixp4xx_config_addr(u8 bus_num, u16 devfn, int where)
{
	u32 addr;
	if (!bus_num) {
		
		addr = BIT(32-PCI_SLOT(devfn)) | ((PCI_FUNC(devfn)) << 8) | 
		    (where & ~3);	
	} else {
		
		addr = (bus_num << 16) | ((PCI_SLOT(devfn)) << 11) | 
			((PCI_FUNC(devfn)) << 8) | (where & ~3) | 1;
	}
	return addr;
}

static u32 bytemask[] = {
		0,
		0xff,
		0xffff,
		0,
		0xffffffff,
};

static u32 local_byte_lane_enable_bits(u32 n, int size)
{
	if (size == 1)
		return (0xf & ~BIT(n)) << CRP_AD_CBE_BESL;
	if (size == 2)
		return (0xf & ~(BIT(n) | BIT(n+1))) << CRP_AD_CBE_BESL;
	if (size == 4)
		return 0;
	return 0xffffffff;
}

static int local_read_config(int where, int size, u32 *value)
{ 
	u32 n, data;
	pr_debug("local_read_config from %d size %d\n", where, size);
	n = where % 4;
	crp_read(where & ~3, &data);
	*value = (data >> (8*n)) & bytemask[size];
	pr_debug("local_read_config read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

static int local_write_config(int where, int size, u32 value)
{
	u32 n, byte_enables, data;
	pr_debug("local_write_config %#x to %d size %d\n", value, where, size);
	n = where % 4;
	byte_enables = local_byte_lane_enable_bits(n, size);
	if (byte_enables == 0xffffffff)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	data = value << (8*n);
	crp_write((where & ~3) | byte_enables, data);
	return PCIBIOS_SUCCESSFUL;
}

static u32 byte_lane_enable_bits(u32 n, int size)
{
	if (size == 1)
		return (0xf & ~BIT(n)) << 4;
	if (size == 2)
		return (0xf & ~(BIT(n) | BIT(n+1))) << 4;
	if (size == 4)
		return 0;
	return 0xffffffff;
}

static int ixp4xx_pci_read_config(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *value)
{
	u32 n, byte_enables, addr, data;
	u8 bus_num = bus->number;

	pr_debug("read_config from %d size %d dev %d:%d:%d\n", where, size,
		bus_num, PCI_SLOT(devfn), PCI_FUNC(devfn));

	*value = 0xffffffff;
	n = where % 4;
	byte_enables = byte_lane_enable_bits(n, size);
	if (byte_enables == 0xffffffff)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	addr = ixp4xx_config_addr(bus_num, devfn, where);
	if (ixp4xx_pci_read(addr, byte_enables | NP_CMD_CONFIGREAD, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	*value = (data >> (8*n)) & bytemask[size];
	pr_debug("read_config_byte read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

static int ixp4xx_pci_write_config(struct pci_bus *bus,  unsigned int devfn, int where, int size, u32 value)
{
	u32 n, byte_enables, addr, data;
	u8 bus_num = bus->number;

	pr_debug("write_config_byte %#x to %d size %d dev %d:%d:%d\n", value, where,
		size, bus_num, PCI_SLOT(devfn), PCI_FUNC(devfn));

	n = where % 4;
	byte_enables = byte_lane_enable_bits(n, size);
	if (byte_enables == 0xffffffff)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	addr = ixp4xx_config_addr(bus_num, devfn, where);
	data = value << (8*n);
	if (ixp4xx_pci_write(addr, byte_enables | NP_CMD_CONFIGWRITE, data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops ixp4xx_ops = {
	.read =  ixp4xx_pci_read_config,
	.write = ixp4xx_pci_write_config,
};

static int abort_handler(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	u32 isr, status;

	isr = *PCI_ISR;
	local_read_config(PCI_STATUS, 2, &status);
	pr_debug("PCI: abort_handler addr = %#lx, isr = %#x, "
		"status = %#x\n", addr, isr, status);

	    
	*PCI_ISR = PCI_ISR_PFE;
	status |= PCI_STATUS_REC_MASTER_ABORT;
	local_write_config(PCI_STATUS, 2, status);

	if (fsr & (1 << 10))
		regs->ARM_pc += 4;

	return 0;
}


static int ixp4xx_needs_bounce(struct device *dev, dma_addr_t dma_addr, size_t size)
{
	return (dma_addr + size) >= SZ_64M;
}

static int ixp4xx_pci_platform_notify(struct device *dev)
{
	if(dev->bus == &pci_bus_type) {
		*dev->dma_mask =  SZ_64M - 1;
		dev->coherent_dma_mask = SZ_64M - 1;
		dmabounce_register_dev(dev, 2048, 4096, ixp4xx_needs_bounce);
	}
	return 0;
}

static int ixp4xx_pci_platform_notify_remove(struct device *dev)
{
	if(dev->bus == &pci_bus_type) {
		dmabounce_unregister_dev(dev);
	}
	return 0;
}

void __init ixp4xx_pci_preinit(void)
{
	unsigned long cpuid = read_cpuid_id();

#ifdef CONFIG_IXP4XX_INDIRECT_PCI
	pcibios_min_mem = 0x10000000; 
#else
	pcibios_min_mem = 0x48000000; 
#endif
	if (!(cpuid & 0xf) && cpu_is_ixp42x()) {
		printk("PCI: IXP42x A0 silicon detected - "
			"PCI Non-Prefetch Workaround Enabled\n");
		ixp4xx_pci_read = ixp4xx_pci_read_errata;
	} else
		ixp4xx_pci_read = ixp4xx_pci_read_no_errata;


	
	hook_fault_code(16+6, abort_handler, SIGBUS, 0,
			"imprecise external abort");

	pr_debug("setup PCI-AHB(inbound) and AHB-PCI(outbound) address mappings\n");

	*PCI_PCIMEMBASE = 0x48494A4B;

	*PCI_AHBMEMBASE = (PHYS_OFFSET & 0xFF000000) +
		((PHYS_OFFSET & 0xFF000000) >> 8) +
		((PHYS_OFFSET & 0xFF000000) >> 16) +
		((PHYS_OFFSET & 0xFF000000) >> 24) +
		0x00010203;

	if (*PCI_CSR & PCI_CSR_HOST) {
		printk("PCI: IXP4xx is host\n");

		pr_debug("setup BARs in controller\n");

		local_write_config(PCI_BASE_ADDRESS_0, 4, PHYS_OFFSET);
		local_write_config(PCI_BASE_ADDRESS_1, 4, PHYS_OFFSET + SZ_16M);
		local_write_config(PCI_BASE_ADDRESS_2, 4, PHYS_OFFSET + SZ_32M);
		local_write_config(PCI_BASE_ADDRESS_3, 4,
					PHYS_OFFSET + SZ_32M + SZ_16M);

		local_write_config(PCI_BASE_ADDRESS_4, 4, PHYS_OFFSET + SZ_64M);

		local_write_config(PCI_BASE_ADDRESS_5, 4, 0xfffffc01);
	} else {
		printk("PCI: IXP4xx is target - No bus scan performed\n");
	}

	printk("PCI: IXP4xx Using %s access for memory space\n",
#ifndef CONFIG_IXP4XX_INDIRECT_PCI
			"direct"
#else
			"indirect"
#endif
		);

	pr_debug("clear error bits in ISR\n");
	*PCI_ISR = PCI_ISR_PSE | PCI_ISR_PFE | PCI_ISR_PPE | PCI_ISR_AHBE;

#ifdef __ARMEB__
	*PCI_CSR = PCI_CSR_IC | PCI_CSR_ABE | PCI_CSR_PDS | PCI_CSR_ADS;
#else
	*PCI_CSR = PCI_CSR_IC | PCI_CSR_ABE;
#endif

	pr_debug("DONE\n");
}

int ixp4xx_setup(int nr, struct pci_sys_data *sys)
{
	struct resource *res;

	if (nr >= 1)
		return 0;

	res = kzalloc(sizeof(*res) * 2, GFP_KERNEL);
	if (res == NULL) {
		panic("PCI: unable to allocate resources?\n");
	}

	local_write_config(PCI_COMMAND, 2, PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY);

	res[0].name = "PCI I/O Space";
	res[0].start = 0x00000000;
	res[0].end = 0x0000ffff;
	res[0].flags = IORESOURCE_IO;

	res[1].name = "PCI Memory Space";
	res[1].start = PCIBIOS_MIN_MEM;
	res[1].end = PCIBIOS_MAX_MEM;
	res[1].flags = IORESOURCE_MEM;

	request_resource(&ioport_resource, &res[0]);
	request_resource(&iomem_resource, &res[1]);

	pci_add_resource_offset(&sys->resources, &res[0], sys->io_offset);
	pci_add_resource_offset(&sys->resources, &res[1], sys->mem_offset);

	platform_notify = ixp4xx_pci_platform_notify;
	platform_notify_remove = ixp4xx_pci_platform_notify_remove;

	return 1;
}

struct pci_bus * __devinit ixp4xx_scan_bus(int nr, struct pci_sys_data *sys)
{
	return pci_scan_root_bus(NULL, sys->busnr, &ixp4xx_ops, sys,
				 &sys->resources);
}

int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	if (mask >= SZ_64M - 1)
		return 0;

	return -EIO;
}

EXPORT_SYMBOL(ixp4xx_pci_read);
EXPORT_SYMBOL(ixp4xx_pci_write);
EXPORT_SYMBOL(dma_set_coherent_mask);
