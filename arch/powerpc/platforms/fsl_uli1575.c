/*
 * ULI M1575 setup code - specific to Freescale boards
 *
 * Copyright 2007 Freescale Semiconductor Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mc146818rtc.h>

#include <asm/pci-bridge.h>

#define ULI_PIRQA	0x08
#define ULI_PIRQB	0x09
#define ULI_PIRQC	0x0a
#define ULI_PIRQD	0x0b
#define ULI_PIRQE	0x0c
#define ULI_PIRQF	0x0d
#define ULI_PIRQG	0x0e

#define ULI_8259_NONE	0x00
#define ULI_8259_IRQ1	0x08
#define ULI_8259_IRQ3	0x02
#define ULI_8259_IRQ4	0x04
#define ULI_8259_IRQ5	0x05
#define ULI_8259_IRQ6	0x07
#define ULI_8259_IRQ7	0x06
#define ULI_8259_IRQ9	0x01
#define ULI_8259_IRQ10	0x03
#define ULI_8259_IRQ11	0x09
#define ULI_8259_IRQ12	0x0b
#define ULI_8259_IRQ14	0x0d
#define ULI_8259_IRQ15	0x0f

u8 uli_pirq_to_irq[8] = {
	ULI_8259_IRQ9,		
	ULI_8259_IRQ10,		
	ULI_8259_IRQ11,		
	ULI_8259_IRQ12,		
	ULI_8259_IRQ5,		
	ULI_8259_IRQ6,		
	ULI_8259_IRQ7,		
	ULI_8259_NONE,		
};

static inline bool is_quirk_valid(void)
{
	return (machine_is(mpc86xx_hpcn) ||
		machine_is(mpc8544_ds) ||
		machine_is(p2020_ds) ||
		machine_is(mpc8572_ds));
}

static void __devinit early_uli5249(struct pci_dev *dev)
{
	unsigned char temp;

	if (!is_quirk_valid())
		return;

	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_IO |
		 PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	
	pci_read_config_byte(dev, 0x7c, &temp);
	pci_write_config_byte(dev, 0x7c, 0x80);

	
	pci_write_config_byte(dev, PCI_CLASS_PROG, 0x01);
	dev->class |= 0x1;

	
	pci_write_config_byte(dev, 0x7c, temp);
}


static void __devinit quirk_uli1575(struct pci_dev *dev)
{
	int i;

	if (!is_quirk_valid())
		return;


	
	for (i = 0; i < 4; i++) {
		u8 val = uli_pirq_to_irq[i*2] | (uli_pirq_to_irq[i*2+1] << 4);
		pci_write_config_byte(dev, 0x48 + i, val);
	}

	
	pci_write_config_byte(dev, 0x86, ULI_PIRQD);

	
	pci_write_config_byte(dev, 0x87, ULI_PIRQA);

	
	pci_write_config_byte(dev, 0x88, ULI_PIRQB);

	
	pci_write_config_byte(dev, 0x89, ULI_PIRQF);

	
	pci_write_config_byte(dev, 0x8a, ULI_PIRQF);

	
	pci_write_config_byte(dev, 0x8b, ULI_PIRQF);

	
	pci_write_config_byte(dev, 0x8c, ULI_PIRQF);

	
	pci_write_config_byte(dev, 0x8d, ULI_PIRQE);

	
	pci_write_config_byte(dev, 0x8e, ULI_PIRQG);

	
	pci_write_config_byte(dev, 0x8f, ULI_PIRQG);

	
	pci_write_config_byte(dev, 0x74, ULI_8259_IRQ11);

	pci_write_config_byte(dev, 0x44, 0x30 | ULI_8259_IRQ14);
	pci_write_config_byte(dev, 0x75, ULI_8259_IRQ15);
}

static void __devinit quirk_final_uli1575(struct pci_dev *dev)
{
	if (!is_quirk_valid())
		return;

	outb(0xfa, 0x4d0);
	outb(0x1e, 0x4d1);

	
	CMOS_WRITE(RTC_SET, RTC_CONTROL);
	CMOS_WRITE(RTC_24H, RTC_CONTROL);

	
	CMOS_WRITE(0, RTC_VALID);

	outb_p(0x7c, 0x72);
	outb_p(RTC_ALARM_DONT_CARE, 0x73);

	outb_p(0x7d, 0x72);
	outb_p(RTC_ALARM_DONT_CARE, 0x73);
}

static void __devinit quirk_uli5288(struct pci_dev *dev)
{
	unsigned char c;
	unsigned int d;

	if (!is_quirk_valid())
		return;

	
	pci_read_config_byte(dev, 0x83, &c);
	pci_write_config_byte(dev, 0x83, c|0x80);

	pci_read_config_dword(dev, PCI_CLASS_REVISION, &d);
	d = (d & 0xff) | (PCI_CLASS_STORAGE_SATA_AHCI << 8);
	pci_write_config_dword(dev, PCI_CLASS_REVISION, d);

	
	pci_write_config_byte(dev, 0x83, c);

	
	pci_read_config_byte(dev, 0x84, &c);
	pci_write_config_byte(dev, 0x84, c & ~0x01);
}

static void __devinit quirk_uli5229(struct pci_dev *dev)
{
	unsigned short temp;

	if (!is_quirk_valid())
		return;

	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_INTX_DISABLE |
		PCI_COMMAND_MASTER | PCI_COMMAND_IO);

	
	pci_read_config_word(dev, 0x4a, &temp);
	pci_write_config_word(dev, 0x4a, temp | 0x1000);
}

static void __devinit quirk_final_uli5249(struct pci_dev *dev)
{
	int i;
	u8 *dummy;
	struct pci_bus *bus = dev->bus;
	struct resource *res;
	resource_size_t end = 0;

	for (i = PCI_BRIDGE_RESOURCES; i < PCI_BRIDGE_RESOURCES+3; i++) {
		unsigned long flags = pci_resource_flags(dev, i);
		if ((flags & (IORESOURCE_MEM|IORESOURCE_PREFETCH)) == IORESOURCE_MEM)
			end = pci_resource_end(dev, i);
	}

	pci_bus_for_each_resource(bus, res, i) {
		if (res && res->flags & IORESOURCE_MEM) {
			if (res->end == end)
				dummy = ioremap(res->start, 0x4);
			else
				dummy = ioremap(res->end - 3, 0x4);
			if (dummy) {
				in_8(dummy);
				iounmap(dummy);
			}
			break;
		}
	}
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_AL, 0x5249, early_uli5249);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL, 0x1575, quirk_uli1575);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL, 0x5288, quirk_uli5288);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL, 0x5229, quirk_uli5229);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL, 0x5249, quirk_final_uli5249);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL, 0x1575, quirk_final_uli1575);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_AL, 0x5229, quirk_uli5229);

static void __devinit hpcd_quirk_uli1575(struct pci_dev *dev)
{
	u32 temp32;

	if (!machine_is(mpc86xx_hpcd))
		return;

	
	pci_read_config_dword(dev, 0x48, &temp32);
	pci_write_config_dword(dev, 0x48, (temp32 | 1<<26));

	
	pci_read_config_dword(dev, 0x90, &temp32);
	pci_write_config_dword(dev, 0x90, (temp32 | 1<<22));
}

static void __devinit hpcd_quirk_uli5288(struct pci_dev *dev)
{
	unsigned char c;

	if (!machine_is(mpc86xx_hpcd))
		return;

	pci_read_config_byte(dev, 0x83, &c);
	c |= 0x80;
	pci_write_config_byte(dev, 0x83, c);

	pci_write_config_byte(dev, PCI_CLASS_PROG, 0x01);
	pci_write_config_byte(dev, PCI_CLASS_DEVICE, 0x06);

	pci_read_config_byte(dev, 0x83, &c);
	c &= 0x7f;
	pci_write_config_byte(dev, 0x83, c);
}

static void __devinit hpcd_quirk_uli5229(struct pci_dev *dev)
{
	unsigned char c;

	if (!machine_is(mpc86xx_hpcd))
		return;

	pci_read_config_byte(dev, 0x4b, &c);
	c |= 0x10;
	pci_write_config_byte(dev, 0x4b, c);
}

static void __devinit hpcd_final_uli5288(struct pci_dev *dev)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	struct device_node *hosenode = hose ? hose->dn : NULL;
	struct of_irq oirq;
	int virq, pin = 2;
	u32 laddr[3];

	if (!machine_is(mpc86xx_hpcd))
		return;

	if (!hosenode)
		return;

	laddr[0] = (hose->first_busno << 16) | (PCI_DEVFN(31, 0) << 8);
	laddr[1] = laddr[2] = 0;
	of_irq_map_raw(hosenode, &pin, 1, laddr, &oirq);
	virq = irq_create_of_mapping(oirq.controller, oirq.specifier,
				     oirq.size);
	dev->irq = virq;
}

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL, 0x1575, hpcd_quirk_uli1575);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL, 0x5288, hpcd_quirk_uli5288);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL, 0x5229, hpcd_quirk_uli5229);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL, 0x5288, hpcd_final_uli5288);

int uli_exclude_device(struct pci_controller *hose,
			u_char bus, u_char devfn)
{
	if (bus == (hose->first_busno + 2)) {
		
		if ((PCI_SLOT(devfn) == 29) && (PCI_FUNC(devfn) == 1))
			return PCIBIOS_DEVICE_NOT_FOUND;

		
		if ((PCI_SLOT(devfn) == 29) && (PCI_FUNC(devfn) == 2))
			return PCIBIOS_DEVICE_NOT_FOUND;
	}

	return PCIBIOS_SUCCESSFUL;
}
