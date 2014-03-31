/*
 * Copyright (C) 2008 Lemote Technology
 * Copyright (C) 2004 ICT CAS
 * Author: Li xiaoyu, lixy@ict.ac.cn
 *
 * Copyright (C) 2007 Lemote, Inc.
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/init.h>
#include <linux/pci.h>

#include <loongson.h>
#include <cs5536/cs5536.h>
#include <cs5536/cs5536_pci.h>


#define PCIA		4
#define PCIB		5
#define PCIC		6
#define PCID		7

static char irq_tab[][5] __initdata = {
	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, PCIA, 0, 0, 0},	
	{0, PCIB, 0, 0, 0},	
	{0, PCIC, 0, 0, 0},	
	{0, PCID, 0, 0, 0},	
	{0, PCIA, PCIB, PCIC, PCID},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
	{0, 0, 0, 0, 0},	
};

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	int virq;

	if ((PCI_SLOT(dev->devfn) != PCI_IDSEL_CS5536)
	    && (PCI_SLOT(dev->devfn) < 32)) {
		virq = irq_tab[slot][pin];
		printk(KERN_INFO "slot: %d, pin: %d, irq: %d\n", slot, pin,
		       virq + LOONGSON_IRQ_BASE);
		if (virq != 0)
			return LOONGSON_IRQ_BASE + virq;
		else
			return 0;
	} else if (PCI_SLOT(dev->devfn) == PCI_IDSEL_CS5536) {	
		switch (PCI_FUNC(dev->devfn)) {
		case 2:
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
					      CS5536_IDE_INTR);
			return CS5536_IDE_INTR;	
		case 3:
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
					      CS5536_ACC_INTR);
			return CS5536_ACC_INTR;	
		case 4:	
		case 5:	
		case 6:	
		case 7:	
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
					      CS5536_USB_INTR);
			return CS5536_USB_INTR;
		}
		return dev->irq;
	} else {
		printk(KERN_INFO " strange pci slot number.\n");
		return 0;
	}
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

static void __init loongson_cs5536_isa_fixup(struct pci_dev *pdev)
{
	
	pci_write_config_dword(pdev, PCI_UART1_INT_REG, 1);
	pci_write_config_dword(pdev, PCI_UART2_INT_REG, 1);
}

static void __init loongson_cs5536_ide_fixup(struct pci_dev *pdev)
{
	
	pci_write_config_dword(pdev, PCI_IDE_CFG_REG,
			       CS5536_IDE_FLASH_SIGNATURE);
}

static void __init loongson_cs5536_acc_fixup(struct pci_dev *pdev)
{
	
	pci_write_config_dword(pdev, PCI_ACC_INT_REG, 1);

	pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0xc0);
}

static void __init loongson_cs5536_ohci_fixup(struct pci_dev *pdev)
{
	
	
	pci_write_config_dword(pdev, PCI_OHCI_INT_REG, 1);
}

static void __init loongson_cs5536_ehci_fixup(struct pci_dev *pdev)
{
	u32 hi, lo;

	
	_rdmsr(USB_MSR_REG(USB_CONFIG), &hi, &lo);
	_wrmsr(USB_MSR_REG(USB_CONFIG), (1 << 1) | (1 << 3), lo);

	
	pci_write_config_dword(pdev, PCI_EHCI_FLADJ_REG, 0x2000);
}

static void __init loongson_nec_fixup(struct pci_dev *pdev)
{
	unsigned int val;

	pci_read_config_dword(pdev, 0xe0, &val);
	
	pci_write_config_dword(pdev, 0xe0, (val & ~3) | 0x2);
}

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_ISA,
			 loongson_cs5536_isa_fixup);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_OHC,
			 loongson_cs5536_ohci_fixup);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_EHC,
			 loongson_cs5536_ehci_fixup);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_AUDIO,
			 loongson_cs5536_acc_fixup);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_IDE,
			 loongson_cs5536_ide_fixup);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NEC, PCI_DEVICE_ID_NEC_USB,
			 loongson_nec_fixup);
